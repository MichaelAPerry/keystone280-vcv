// Behringer 904A Voltage Controlled Low Pass Filter.
// Source: Quick Start Guide and the model specification in sim/docs/m3-specs.md.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Confidence.hpp"

namespace k280 {

struct Lpf904ACalibration {
    struct FixedCvPoint {
        float knob;
        float middleHz;
    };

    // The 2026-11-22 sweep supersedes test 5's 725 Hz at noon.
    std::array<float, 3> rangeCentreHz = {355.25f, 1421.f, 5684.f};
    // bench 2026-11-22, t904A_fixed_law2
    std::array<FixedCvPoint, 8> fixedCvTable = {{
        {0.f, 63.f}, {0.2f, 146.5f}, {0.3f, 331.f}, {0.4f, 801.f},
        {0.5f, 1421.f}, {0.6f, 2416.f}, {0.7f, 3797.f}, {0.8f, 7205.f},
    }};

    // ASSUMPTION: model limits, regeneration taper, and saturation levels.
    float controlLimitV = 7.f;
    float nyquistGuard = 0.45f;  // fraction of host sample rate
    // Bench 2026-11-23 (bench-results.md, kocmoc LADR comparison): rack patch 1
    // harmonics fit a four-pole ladder within 0.2–0.5 dB. At equal knob settings
    // the old 2.06 ratio was about an octave brighter than LADR and the rack.
    // The knob table holds the self-oscillation pitch (noon 1421 Hz). The
    // rackcheck filtersweep at REGEN 0 (per-take saw lock) puts the four-pole
    // -3 dB corner at 352.5 Hz at noon, i.e. poles at 810.4 Hz = 0.570 x the
    // self-oscillation pitch. The pole ratio slides in log frequency from that
    // value at REGEN 0 to 1.0 at oscillation onset. bench 2026-11-23, filtersweep_163544
    float passbandPoleRatio = 0.5703f;
    float oscillationRegen = 0.665f;  // bench 2026-11-21, test 5b
    float regenKnee = 0.9f;
    float fullFeedback = 4.85f;
    float feedbackAtKnee = 4.65f;
    float inputDriveV = 4.f;
    std::array<float, 4> stageDriveV = {23.f, 16.f, 11.f, 8.f};
    float feedbackDriveV = 1.4f;  // bench 2026-11-21, test 5
    float trackingOffset = 0.00075f;
    float trackingSlope = 0.19f;  // bench 2026-11-21, test 5
    float acCouplingHz = 2.f;
    float outputLimitV = 10.f;
    float startupNoiseV = 1e-6f;
};

class Lpf904A {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Params {
        float fixedCv = 0.5f;
        int range = 1;
        float regen = 0.f;
    };

    struct Inputs {
        float in = 0.f;
        float cv[3] = {};
    };

    struct Outputs {
        float out = 0.f;
    };

    Lpf904ACalibration cal;

    // Panel pole frequency before applying the host sample-rate limit in process().
    float cutoffHz(const Params& p, float cvSum) const {
        const float knob = std::clamp(p.fixedCv, 0.f, 1.f);
        const auto& table = cal.fixedCvTable;
        // Use the last segment for extrapolation through the end of the knob travel.
        const auto upper = std::lower_bound(table.begin() + 1, table.end() - 1, knob,
            [](const auto& point, float value) { return point.knob < value; });
        const auto& lower = *(upper - 1);
        const float fraction = (knob - lower.knob) / (upper->knob - lower.knob);
        const float fixedV = std::log2(lower.middleHz / cal.rangeCentreHz[1])
                           + fraction * std::log2(upper->middleHz / lower.middleHz);
        const float volts = std::clamp(fixedV + cvSum, -cal.controlLimitV, cal.controlLimitV);
        return cal.rangeCentreHz[std::clamp(p.range, 0, 2)] * std::exp2(volts);
    }

    void reset() {
        Lpf904A fresh;
        fresh.cal = cal;
        *this = fresh;
    }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        const float hz = std::min(cutoffHz(p, in.cv[0] + in.cv[1] + in.cv[2]),
                                  cal.nyquistGuard * sampleRate);
        const float regen = std::clamp(p.regen, 0.f, 1.f);
        // Each pole transmits 1/sqrt(2) at -45 degrees, so the loop needs gain 4.
        const float feedback = regen <= cal.oscillationRegen
            ? 4.f * regen / cal.oscillationRegen
            : cal.feedbackAtKnee + (regen - cal.regenKnee) *
                (regen < cal.regenKnee
                    ? (cal.feedbackAtKnee - 4.f) / (cal.regenKnee - cal.oscillationRegen)
                    : (cal.fullFeedback - cal.feedbackAtKnee) / (1.f - cal.regenKnee));
        // Saturation and the one-substep lag in the stage secants sharpen the
        // limit cycle. Correct its measured frequency as a function of excess
        // loop gain and normalized cutoff, then prewarp for the 2x ladder.
        const float normalized = hz / sampleRate;
        const float tracking = 1.f - std::max(feedback - 4.f, 0.f) *
            (cal.trackingOffset + cal.trackingSlope * normalized);
        const float poleRatio = std::exp2(std::log2(cal.passbandPoleRatio) *
            (1.f - std::min(regen / cal.oscillationRegen, 1.f)));
        const float g = std::tan(kPi * 0.5f * std::min(normalized * tracking * poleRatio, 0.49f));

        // Interpolate and decimate by two with the two polyphase branches
        // of the same 19-tap half-band FIR. Audio has nine host samples of
        // FIR group delay in total. Cutoff CV is held over each substep pair.
        push(input_, in.in);
        const float noise = startupNoise();
        const float first = step(g, feedback, 2.f * halfBandSide(input_), noise);
        const float second = step(g, feedback, input_[4], noise);
        push(even_, first);
        push(odd_, second);
        const float low = 0.5f * even_[4] + halfBandSide(odd_);

        const double hp = low - dc_;
        // Backward-Euler RC high-pass, with its low-pass state held in double.
        const double w = 2.0 * kPi * cal.acCouplingHz / sampleRate;
        dc_ += w / (1.0 + w) * hp;
        return {cal.outputLimitV * saturate(float(hp / (1.0 + w)) / cal.outputLimitV)};
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    // Rational tanh approximation, monotone on [-3, 3], with unit small-signal gain.
    static float tanhSlope(float x) {
        const float square = x * x;
        if (square > 9.f)
            return 1.f / std::abs(x);
        return (27.f + square) / (27.f + 9.f * square);
    }

    static float saturate(float x) {
        x = std::clamp(x, -3.f, 3.f);
        return x * tanhSlope(x);
    }

    template <std::size_t Size>
    static void push(std::array<float, Size>& history, float sample) {
        for (std::size_t i = Size - 1; i > 0; --i)
            history[i] = history[i - 1];
        history[0] = sample;
    }

    // Hamming-windowed half-band coefficients, normalized for unity DC gain
    // when the center tap (0.5) is included.
    static float halfBandSide(const std::array<float, 10>& history) {
        return 0.30779272f * (history[4] + history[5])
            - 0.08125424f * (history[3] + history[6])
            + 0.02913261f * (history[2] + history[7])
            - 0.00848509f * (history[1] + history[8])
            + 0.00281400f * (history[0] + history[9]);
    }

    float startupNoise() {
        noiseState_ ^= noiseState_ << 13;
        noiseState_ ^= noiseState_ >> 17;
        noiseState_ ^= noiseState_ << 5;
        return cal.startupNoiseV * (float(noiseState_ >> 8) * (1.f / 8388608.f) - 1.f);
    }

    float step(float g, float feedback, float input, float noise) {
        // Each trapezoidal pole is affine in its input with the preceding
        // substep's tanh secant held constant for the duration of the solve.
        const float G = g / (1.f + g);
        const float memory = 1.f - G;
        std::array<float, 4> gain;
        std::array<float, 4> offset;
        for (std::size_t i = 0; i < gain.size(); ++i) {
            gain[i] = G * tanhSlope(stageInput_[i] / cal.stageDriveV[i]);
            offset[i] = memory * state_[i];
        }

        const float drive = cal.inputDriveV * saturate(input / cal.inputDriveV) + noise;
        const float chain = gain[3] * gain[2] * gain[1] * gain[0];
        const float open = offset[3] + gain[3] *
            (offset[2] + gain[2] * (offset[1] + gain[1] * (offset[0] + gain[0] * drive)));
        // One quasi-Newton step on y + loop * shape(y) = open.
        // Use the tanh derivative with the rational approximation in its place.
        const float shaped = saturate(last_ / cal.feedbackDriveV);
        const float loop = chain * feedback;
        const float derivative = loop * (1.f - shaped * shaped);
        const float y = (open - loop * cal.feedbackDriveV * shaped + derivative * last_)
                      / (1.f + derivative);
        float stage = drive - feedback * cal.feedbackDriveV * saturate(y / cal.feedbackDriveV);
        for (std::size_t i = 0; i < gain.size(); ++i) {
            stageInput_[i] = stage;
            stage = gain[i] * stage + offset[i];
            // Discard values far below the noise floor to avoid subnormal arithmetic.
            state_[i] = 2.f * stage - state_[i];
            if (std::abs(state_[i]) < 1e-15f)
                state_[i] = 0.f;
        }
        last_ = stage;
        return stage;
    }

    std::array<float, 4> state_{};
    std::array<float, 4> stageInput_{};
    std::array<float, 5> even_{};
    std::array<float, 10> odd_{};
    std::array<float, 10> input_{};
    std::uint32_t noiseState_ = 1u;
    float last_ = 0.f;
    double dc_ = 0.0;
};

}  // namespace k280
