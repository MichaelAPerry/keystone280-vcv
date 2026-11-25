// Behringer CP3A-M Mixer. See sim/docs/m4-specs.md.
#pragma once

#include <array>
#include <cmath>

#include "Confidence.hpp"

namespace k280 {

struct Mixer3Calibration {
    struct Point {
        float knob;
        float value;
    };

    // ASSUMPTION: linear channel law, interpolated between dial marks.
    std::array<Point, 11> gainTable{{
        {0.f, 0.f}, {0.1f, 0.1f}, {0.2f, 0.2f}, {0.3f, 0.3f},
        {0.4f, 0.4f}, {0.5f, 0.5f}, {0.6f, 0.6f}, {0.7f, 0.7f},
        {0.8f, 0.8f}, {0.9f, 0.9f}, {1.f, 1.f},
    }};
    std::array<Point, 11> masterTable{{
        {0.f, 0.f},  // bench 2026-11-22, test 12b
        {0.1f, 0.1f},  // bench 2026-11-22, test 12b
        {0.2f, 0.2f},  // bench 2026-11-22, test 12b
        {0.3f, 0.3f},  // bench 2026-11-22, test 12b
        {0.4f, 0.4f},  // bench 2026-11-22, test 12b
        {0.5f, 0.5f},  // bench 2026-11-22, test 12b
        {0.6f, 0.6f},  // bench 2026-11-22, test 12b
        {0.7f, 0.7f},  // bench 2026-11-22, test 12b
        {0.8f, 0.8f},  // bench 2026-11-22, test 12b
        {0.9f, 0.9f},  // bench 2026-11-22, test 12b
        {1.f, 1.f},  // bench 2026-11-22, test 12b
    }};

    float softKneeV = 6.5f;  // ASSUMPTION
    float softClipV = 8.f;   // ASSUMPTION: rounded from the QSG maximum
};

class Mixer3 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Params {
        float gain[4]{};
        float master = 0.5f;
    };

    struct Inputs {
        float in[4]{};
        float maIn = 0.f;
        float mbIn = 0.f;
    };

    struct Outputs {
        float outP = 0.f;
        float outN = 0.f;
        float ma = 0.f;
        float mb = 0.f;
    };

    Mixer3Calibration cal;

    Outputs process(float /*sampleRate*/, const Params& p, const Inputs& in) const {
        double sum = 0.0;
        for (int channel = 0; channel < 4; ++channel)
            sum += double(lookup(cal.gainTable, p.gain[channel])) * in.in[channel];

        const float out = softClip(lookup(cal.masterTable, p.master) * sum);
        return {out, -out, in.maIn, in.mbIn};
    }

private:
    static float lookup(const std::array<Mixer3Calibration::Point, 11>& table, float knob) {
        if (knob <= table.front().knob) return table.front().value;
        for (std::size_t i = 1; i < table.size(); ++i) {
            const auto& lo = table[i - 1];
            const auto& hi = table[i];
            if (knob == hi.knob) return hi.value;
            if (knob < hi.knob) {
                const float t = (knob - lo.knob) / (hi.knob - lo.knob);
                return lo.value + t * (hi.value - lo.value);
            }
        }
        return table.back().value;
    }

    // The tanh shoulder joins the linear region with slope 1.
    float softClip(double signal) const {
        const double magnitude = std::fabs(signal);
        if (magnitude <= cal.softKneeV) return static_cast<float>(signal);
        const double room = double(cal.softClipV) - cal.softKneeV;
        return static_cast<float>(std::copysign(
            cal.softKneeV + room * std::tanh((magnitude - cal.softKneeV) / room), signal));
    }
};

}  // namespace k280
