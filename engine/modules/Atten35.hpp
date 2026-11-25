// Behringer CP35 Attenuators / Multiples / ±6 V. See sim/docs/m4-specs.md.
#pragma once

#include <array>

#include "Confidence.hpp"

namespace k280 {

struct Atten35Calibration {
    struct Point {
        float knob;
        float value;
    };

    // Measured transfer ratios, interpolated between dial marks.
    std::array<Point, 6> taperTable{{
        {0.f, 0.f},  // bench 2026-11-22, test 4
        {0.2f, 0.079f},  // bench 2026-11-22, test 4
        {0.4f, 0.202f},  // bench 2026-11-22, test 4
        {0.6f, 0.333f},  // bench 2026-11-22, test 4
        {0.8f, 0.443f},  // bench 2026-11-22, test 4
        {1.f, 0.564f},  // bench 2026-11-23, filtersweep_163544
    }};
};

class Atten35 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Params {
        float att[4]{};
    };

    struct Inputs {
        float ain[4]{};
        bool ainConnected[4]{};
        float maIn = 0.f;
        float mbIn = 0.f;
    };

    struct Outputs {
        float aout[4]{};
        float n6 = -6.f;
        float p6 = 6.f;
        float ma = 0.f;
        float mb = 0.f;
    };

    Atten35Calibration cal;

    Outputs process(float /*sampleRate*/, const Params& p, const Inputs& in) const {
        Outputs out;
        float signal = 0.f;
        for (int channel = 0; channel < 4; ++channel) {
            if (in.ainConnected[channel]) signal = in.ain[channel];
            out.aout[channel] = signal * taper(p.att[channel]);
        }
        out.ma = in.maIn;
        out.mb = in.mbIn;
        return out;
    }

private:
    float taper(float knob) const {
        const auto& table = cal.taperTable;
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
};

}  // namespace k280
