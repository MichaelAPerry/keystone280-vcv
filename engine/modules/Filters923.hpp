// Behringer 923 Filters / Noise. See sim/docs/m4-specs.md.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>

#include "Confidence.hpp"

namespace k280 {

struct Filters923Calibration {
    struct FrequencyPoint {
        float knob;
        float hz;
    };

    // ASSUMPTION: sample 10 Hz * 1000^knob at tenths and interpolate logarithmically.
    std::array<FrequencyPoint, 11> cutoffTable{{
        {0.f, 10.f}, {.1f, 19.952623f}, {.2f, 39.810717f}, {.3f, 79.432823f},
        {.4f, 158.489319f}, {.5f, 316.227766f}, {.6f, 630.957344f},
        {.7f, 1258.925412f}, {.8f, 2511.886432f}, {.9f, 5011.872336f},
        {1.f, 10000.f},
    }};
    float lpCutoffScale = .6f;  // bench 2026-11-21, test 6a
    float hpCutoffScale = .47f;  // bench 2026-11-21, test 6b
    float nyquistGuard = .45f; // ASSUMPTION: maximum cutoff as a fraction of sample rate.
    double noiseCouplingHz = 5.0; // ASSUMPTION: AC coupling corner from the model spec.

    // ASSUMPTION: octave-spaced pinking shelves with a half-octave pole/zero separation.
    static constexpr std::size_t pinkShelfCount = 18;
    double pinkFirstPoleHz = 1.0;
    double pinkZeroRatio = 1.4142135623730951;
    // ASSUMPTION: two-tap correction for matched-z flattening near Nyquist.
    double pinkHighFrequencyCorrection = .1;

    float whiteRmsV = .73f;  // bench 2026-11-21, test 7
    float pinkRmsV = 1.14f;  // bench 2026-11-21, test 7b
};

class Filters923 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Params {
        float lp = 0.f;
        float hp = 0.f;
    };

    struct Inputs {
        float lpIn = 0.f;
        float hpIn = 0.f;
    };

    struct Outputs {
        float lpOut = 0.f;
        float hpOut = 0.f;
        float white = 0.f;
        float pink = 0.f;
    };

    Filters923() : Filters923(0) {}
    explicit Filters923(std::uint32_t value) { seed(value); }

    Filters923Calibration cal;

    void seed(std::uint32_t value) {
        std::seed_seq whiteSeed{value, std::uint32_t{0}};
        std::seed_seq pinkSeed{value, std::uint32_t{1}};
        whiteRng_.seed(whiteSeed);
        pinkRng_.seed(pinkSeed);
        whiteCoupling_ = pinkCoupling_ = {};
        pinkPrevious_ = 0.0;
        for (auto& shelf : pinkShelves_) shelf.state = 0.0;
    }

    float cutoffHz(float knob, bool highPass = false) const {
        return dialFrequencyHz(knob) * (highPass ? cal.hpCutoffScale : cal.lpCutoffScale);
    }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        prepareNoise(sampleRate);
        const double lpGain = filterGain(sampleRate, cutoffHz(p.lp));
        const double hpGain = filterGain(sampleRate, cutoffHz(p.hp, true));
        const double lp = lp_.lowPass(in.lpIn, lpGain);
        const double hp = hp_.highPass(in.hpIn, hpGain);
        const double white = whiteCoupling_.highPass(noise(whiteRng_), couplingGain_);

        double pink = noise(pinkRng_);
        for (auto& shelf : pinkShelves_) pink = shelf.process(pink);
        const double corrected = pink + cal.pinkHighFrequencyCorrection * pinkPrevious_;
        pinkPrevious_ = pink;
        pink = pinkCoupling_.highPass(corrected, couplingGain_);

        // Each noise output represents both jacks of its hardware pair.
        return {float(lp), float(hp), float(white * whiteScale_ * cal.whiteRmsV),
                float(pink * pinkScale_ * cal.pinkRmsV)};
    }

private:
    float dialFrequencyHz(float knob) const {
        const auto& table = cal.cutoffTable;
        if (knob <= table.front().knob) return table.front().hz;
        for (std::size_t i = 1; i < table.size(); ++i) {
            const auto& lo = table[i - 1];
            const auto& hi = table[i];
            if (knob == hi.knob) return hi.hz;
            if (knob < hi.knob) {
                const float fraction = (knob - lo.knob) / (hi.knob - lo.knob);
                return lo.hz * std::pow(hi.hz / lo.hz, fraction);
            }
        }
        return table.back().hz;
    }

    static constexpr double kPi = 3.14159265358979323846;

    struct OnePole {
        double state = 0.0;

        double lowPass(double input, double gain) {
            const double delta = (input - state) * gain;
            const double low = state + delta;
            state = low + delta;
            return low;
        }

        double highPass(double input, double gain) {
            return input - lowPass(input, gain);
        }
    };

    struct PinkShelf {
        double pole = 0.0;
        double zero = 0.0;
        double state = 0.0;

        double process(double input) {
            const double output = input + state;
            state = pole * state + (pole - zero) * input;
            return output;
        }
    };

    double filterGain(float sampleRate, double hz) const {
        const double cutoff = std::min(hz, double(cal.nyquistGuard) * sampleRate);
        const double g = std::tan(kPi * cutoff / sampleRate);
        return g / (1.0 + g);
    }

    static double noise(std::mt19937& rng) {
        // Sum two independent uniform draws and scale to unit variance.
        constexpr double bins = 4294967296.0;
        const double first = double(rng());
        const double second = double(rng());
        return (first + second - (bins - 1.0)) * (std::sqrt(6.0) / bins);
    }

    void prepareNoise(float sampleRate) {
        if (sampleRate == noiseSampleRate_ && cal.noiseCouplingHz == couplingHz_ &&
            cal.nyquistGuard == noiseNyquistGuard_ &&
            cal.pinkFirstPoleHz == pinkFirstPoleHz_ && cal.pinkZeroRatio == pinkZeroRatio_ &&
            cal.pinkHighFrequencyCorrection == pinkCorrection_) return;

        noiseSampleRate_ = sampleRate;
        couplingHz_ = cal.noiseCouplingHz;
        noiseNyquistGuard_ = cal.nyquistGuard;
        couplingGain_ = filterGain(sampleRate, couplingHz_);
        pinkFirstPoleHz_ = cal.pinkFirstPoleHz;
        pinkZeroRatio_ = cal.pinkZeroRatio;
        pinkCorrection_ = cal.pinkHighFrequencyCorrection;

        double hz = pinkFirstPoleHz_;
        for (auto& shelf : pinkShelves_) {
            shelf.pole = std::exp(-2.0 * kPi * hz / sampleRate);
            shelf.zero = std::exp(-2.0 * kPi * hz * pinkZeroRatio_ / sampleRate);
            hz *= 2.0;
        }

        // The TPT high-pass has noise power gain 1 - couplingGain.
        whiteScale_ = 1.0 / std::sqrt(1.0 - couplingGain_);
        pinkScale_ = 1.0 / std::sqrt(pinkPower(sampleRate));
    }

    double pinkPower(float sampleRate) const {
        // Integrate squared response over [0, Nyquist] for a unit-variance input.
        // A logarithmic frequency grid resolves the AC corner even at high rates.
        constexpr int bins = 1024;
        const double nyquist = sampleRate / 2.0;
        const double step = std::log1p(nyquist / pinkFirstPoleHz_) / bins;
        const double couplingPole = 1.0 - 2.0 * couplingGain_;
        double power = 0.0;
        for (int i = 0; i < bins; ++i) {
            const double position = (i + .5) * step;
            const double hz = pinkFirstPoleHz_ * std::expm1(position);
            const double sine = std::sin(kPi * hz / sampleRate);
            const double chordSquared = 4.0 * sine * sine;
            const double couplingDirect = 1.0 - couplingGain_;
            double response = couplingDirect * couplingDirect * chordSquared /
                              delayResponsePower(couplingPole, chordSquared);
            for (const auto& shelf : pinkShelves_)
                response *= delayResponsePower(shelf.zero, chordSquared) /
                            delayResponsePower(shelf.pole, chordSquared);
            response *= delayResponsePower(-pinkCorrection_, chordSquared);
            power += response * (hz + pinkFirstPoleHz_);
        }
        return power * step / nyquist;
    }

    static double delayResponsePower(double coefficient, double chordSquared) {
        const double dc = 1.0 - coefficient;
        return dc * dc + coefficient * chordSquared;
    }

    OnePole lp_, hp_, whiteCoupling_, pinkCoupling_;
    std::mt19937 whiteRng_, pinkRng_;
    std::array<PinkShelf, Filters923Calibration::pinkShelfCount> pinkShelves_{};
    float noiseSampleRate_ = 0.f;
    double couplingHz_ = 0.0;
    float noiseNyquistGuard_ = 0.f;
    double couplingGain_ = 0.0;
    double pinkFirstPoleHz_ = 0.0;
    double pinkZeroRatio_ = 0.0;
    double pinkCorrection_ = 0.0;
    double pinkPrevious_ = 0.0;
    double whiteScale_ = 1.0;
    double pinkScale_ = 1.0;
};

}  // namespace k280
