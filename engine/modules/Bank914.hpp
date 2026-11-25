// Behringer 914 Fixed Filter Bank. See sim/docs/m4-specs.md.
#pragma once

#include <array>
#include <cmath>

#include "Confidence.hpp"

namespace k280 {

struct Bank914Calibration {
    struct GainPoint {
        float knob;
        float db;

        bool operator==(const GainPoint& other) const {
            return knob == other.knob && db == other.db;
        }
    };

    // Interpolate linearly in dB between the measured dial positions.
    std::array<GainPoint, 5> levelTable{{
        {0.f, -65.f},   // bench 2026-11-21, test 21
        {.2f, -16.9f},  // bench 2026-11-21, test 21
        {.5f, -7.9f},   // bench 2026-11-21, test 21
        {.8f, -3.f},    // bench 2026-11-21, test 21
        {1.f, 0.f},     // bench 2026-11-21, test 21
    }};
    double inputCouplingHz = 2.0;  // ASSUMPTION: one-pole corner from the model spec.
    float softKneeV = 3.0f;        // ASSUMPTION: transparent up to this level.
    float softClipV = 3.46f;       // +10 dBu peak, rounded as in the model spec.
};

class Bank914 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;
    static constexpr float kBandHz[12] = {
        125.f, 175.f, 250.f, 350.f, 500.f, 700.f,
        1000.f, 1400.f, 2000.f, 2800.f, 4000.f, 5600.f,
    };

    struct Params {
        float lpLevel = 0.f;
        float band[12]{};
        float hpLevel = 0.f;
    };

    struct Inputs {
        float in = 0.f;
    };

    struct Outputs {
        float out = 0.f;     // Both parallel output jacks.
        float inThru = 0.f;  // Raw input node shared by the parallel input jacks.
    };

    Bank914Calibration cal;

    float branchGainDb(float knob) const {
        const auto& table = cal.levelTable;
        if (knob <= table.front().knob) return table.front().db;
        for (std::size_t i = 1; i < table.size(); ++i) {
            const auto& hi = table[i];
            if (knob <= hi.knob) {
                const auto& lo = table[i - 1];
                const float fraction = (knob - lo.knob) / (hi.knob - lo.knob);
                return lo.db + fraction * (hi.db - lo.db);
            }
        }
        return table.back().db;
    }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        if (sampleRate != sampleRate_ || cal.inputCouplingHz != couplingHz_)
            prepare(sampleRate);

        const bool taperChanged = cal.levelTable != levelTable_;
        if (taperChanged) levelTable_ = cal.levelTable;

        const double delta = (in.in - couplingState_) * couplingGain_;
        const double low = couplingState_ + delta;
        couplingState_ = low + delta;
        const double input = in.in - low;

        double sum = branchGain(lpGain_, p.lpLevel, taperChanged) *
                     lp_[1].process(lp_[0].process(input));
        for (std::size_t i = 0; i < bands_.size(); ++i)
            sum += branchGain(bandGains_[i], p.band[i], taperChanged) *
                   bands_[i].process(input);
        sum += branchGain(hpGain_, p.hpLevel, taperChanged) *
               hp_[1].process(hp_[0].process(input));
        return {softClip(static_cast<float>(sum)), in.in};
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kBandQ = 7.0;
    static constexpr double kLpHz = 100.0;
    static constexpr double kHpHz = 7500.0;

    struct Biquad {
        double b0 = 0.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        double process(double x) {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct Gain {
        float knob = -1.f;
        float linear = 0.f;
    };

    float branchGain(Gain& gain, float knob, bool taperChanged) const {
        if (knob != gain.knob || taperChanged || gain.linear == 0.f) {
            gain.knob = knob;
            gain.linear = std::pow(10.f, branchGainDb(knob) / 20.f);
        }
        return gain.linear;
    }

    float softClip(float signal) const {
        const float magnitude = std::fabs(signal);
        if (magnitude <= cal.softKneeV) return signal;
        const float room = cal.softClipV - cal.softKneeV;
        return std::copysign(cal.softKneeV +
                            room * std::tanh((magnitude - cal.softKneeV) / room), signal);
    }

    static Biquad edgeSection(double hz, double rate, double q, bool highPass) {
        const double g = std::tan(kPi * hz / rate);
        const double squared = g * g;
        const double damping = g / q;
        const double scale = 1.0 / (1.0 + damping + squared);
        const double b = highPass ? scale : squared * scale;
        return {b, (highPass ? -2.0 : 2.0) * b, b,
                2.0 * (squared - 1.0) * scale, (1.0 - damping + squared) * scale};
    }

    static std::array<Biquad, 2> edgeBranch(double hz, double rate, bool highPass) {
        if (hz >= rate / 2.0) return {};
        // ASSUMPTION: fourth-order Butterworth shape, normalized to unity at the
        // corner so the level knob sets the gain at the specified LP/HP frequency.
        const double rootTwo = std::sqrt(2.0);
        auto first = edgeSection(hz, rate, 1.0 / std::sqrt(2.0 + rootTwo), highPass);
        first.b0 *= rootTwo;
        first.b1 *= rootTwo;
        first.b2 *= rootTwo;
        return {first, edgeSection(hz, rate, 1.0 / std::sqrt(2.0 - rootTwo), highPass)};
    }

    static Biquad bandBranch(double hz, double rate) {
        if (hz >= rate / 2.0) return {};
        // The digital half-power width is 2 * atan(alpha) radians/sample.
        const double alpha = std::tan(kPi * hz / (rate * kBandQ));
        const double scale = 1.0 / (1.0 + alpha);
        const double b = alpha * scale;
        return {b, 0.0, -b, -2.0 * std::cos(2.0 * kPi * hz / rate) * scale,
                (1.0 - alpha) * scale};
    }

    void prepare(float sampleRate) {
        sampleRate_ = sampleRate;
        couplingHz_ = cal.inputCouplingHz;
        const double g = std::tan(kPi * couplingHz_ / sampleRate);
        couplingGain_ = g / (1.0 + g);
        lp_ = edgeBranch(kLpHz, sampleRate, false);
        hp_ = edgeBranch(kHpHz, sampleRate, true);
        for (std::size_t i = 0; i < bands_.size(); ++i)
            bands_[i] = bandBranch(kBandHz[i], sampleRate);
    }

    float sampleRate_ = 0.f;
    double couplingHz_ = 0.0;
    double couplingGain_ = 0.0;
    double couplingState_ = 0.0;
    std::array<Biquad, 2> lp_{}, hp_{};
    std::array<Biquad, 12> bands_{};
    std::array<Gain, 12> bandGains_{};
    Gain lpGain_, hpGain_;
    decltype(Bank914Calibration::levelTable) levelTable_{};
};

}  // namespace k280
