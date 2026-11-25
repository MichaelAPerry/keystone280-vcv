// Behringer 904B Voltage Controlled High Pass Filter.
// Source: Quick Start Guide and the model specification in sim/docs/m4-specs.md.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Confidence.hpp"

namespace k280 {

struct Hpf904BCalibration {
    std::array<float, 2> rangeCentreHz = {282.8f, 707.1f};
    float fixedSpanV = 12.f;
    float controlLimitV = 7.f;
    float outputLimitV = 10.f;

    // ASSUMPTION: use the 904A host-rate guard and mirror its equal-pole spacing.
    float nyquistGuard = 0.45f;
    // This spacing keeps the corner within 1 dB of -3 dB and the slope
    // within 3 dB/oct of 24 dB/oct between fc/8 and fc/4.
    float poleRatio = 1.f / 2.06f;
    // ASSUMPTION: input tanh drive and output soft-limit knee.
    float inputDriveV = 20.f;
    float outputKneeV = 8.f;
};

class Hpf904B {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Params {
        float fixedCv = 0.5f;
        int range = 0;
    };

    struct Inputs {
        float in = 0.f;
        float cv[3] = {};
    };

    struct Outputs {
        float out = 0.f;
    };

    Hpf904BCalibration cal;

    // Panel frequency before applying the host sample-rate limit in process().
    float cutoffHz(const Params& p, float cvSum) const {
        const float fixed = cal.fixedSpanV * (std::clamp(p.fixedCv, 0.f, 1.f) - 0.5f);
        const float volts = std::clamp(fixed + cvSum, -cal.controlLimitV, cal.controlLimitV);
        return cal.rangeCentreHz[std::clamp(p.range, 0, 1)] * std::exp2(volts);
    }

    void reset() {
        state_.fill(0.0);
    }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        const float hz = std::min(cutoffHz(p, in.cv[0] + in.cv[1] + in.cv[2]),
                                  cal.nyquistGuard * sampleRate);
        // Apply the spacing after prewarping to preserve the combined corner
        // as cutoff approaches Nyquist. Each trapezoidal stage is stable at host rate.
        const double g = cal.poleRatio * std::tan(kPi * hz / sampleRate);
        const double direct = 1.0 / (1.0 + g);
        const double update = 2.0 * g * direct;
        double stage = cal.inputDriveV * std::tanh(double(in.in) / cal.inputDriveV);
        for (double& memory : state_) {
            const double delta = stage - memory;
            stage = direct * delta;
            memory += update * delta;
        }
        return {softClip(float(stage))};
    }

private:
    static constexpr double kPi = 3.14159265358979323846;

    float softClip(float sample) const {
        const float magnitude = std::abs(sample);
        if (magnitude <= cal.outputKneeV)
            return sample;
        const float room = cal.outputLimitV - cal.outputKneeV;
        return std::copysign(cal.outputKneeV + room * std::tanh((magnitude - cal.outputKneeV) / room),
                             sample);
    }

    std::array<double, 4> state_{};
};

}  // namespace k280
