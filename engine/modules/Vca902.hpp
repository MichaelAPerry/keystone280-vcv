// Behringer 902 Voltage Controlled Amplifier.
//
// Source: Behringer 902 Quick Start Guide. CONTROL MODE switches between LIN and EXP.
// FIXED CONTROL VOLTAGE is linear from 0 to 6 V. Two signal inputs (9 kΩ), two signal
// outputs (1.1 kΩ), and three control inputs (100 kΩ, −6 to +6 V).
// The reference and panel identify one VCA with + and − signal jacks.
// ASSUMPTION: subtract the minus input from the plus input. The signal path passes DC,
// and the minus output is the inverse of the plus output.
// EXP uses the active CM1A CV measurements, superseding test 4's passive CP35
// measurements (attenuator voltage metered unloaded).
// ASSUMPTION: the plateau at 4.98 V is unity gain.
// ASSUMPTION: linear up to ±8 V, then a tanh knee that never exceeds ±10 V (rails are ±12 V). Slew only the fixed CV,
// using at most 1 ms for a full knob throw. Calibration values are collected below.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

#include "Confidence.hpp"

namespace k280 {

struct Vca902Calibration {
    float fixedCvMaxV = 6.f;          // QSG
    float linGainPerVolt = 0.56f;  // bench 2026-11-23, linsweep_172245
    float linMuteV = 0.68f;        // bench 2026-11-23, linsweep_172245
    float linMax = 0.93f;            // bench 2026-11-22, test 4
    float linKneeHalfWidth = 0.04f;  // bench 2026-11-22, test 4
    struct ExpPoint {
        float volts;
        float decibels;
    };

    // 1.06 V was at the measurement floor (bench 2026-11-23, CM1A takes B/E).
    float expMuteV = 1.08f;  // ASSUMPTION: start the fade just above the measured floor
    std::array<ExpPoint, 23> expTable{{
        {1.15f, -60.4f},  // bench 2026-11-23, CM1A takes B/E
        {1.23f, -41.0f},  // bench 2026-11-23, CM1A takes B/E
        {1.31f, -30.8f},  // bench 2026-11-23, CM1A takes B/E
        {1.40f, -25.4f},  // bench 2026-11-23, CM1A takes B/E
        {1.48f, -21.8f},  // bench 2026-11-23, CM1A takes B/E
        {1.56f, -19.2f},  // bench 2026-11-23, CM1A takes B/E
        {1.65f, -17.2f},  // bench 2026-11-23, CM1A takes B/E
        {1.73f, -15.6f},  // bench 2026-11-23, CM1A takes B/E
        {1.81f, -14.2f},  // bench 2026-11-23, CM1A takes B/E
        {1.90f, -13.1f},  // bench 2026-11-23, CM1A takes B/E
        {1.98f, -12.0f},  // bench 2026-11-23, CM1A takes B/E
        {2.15f, -10.3f},  // bench 2026-11-23, CM1A takes B/E
        {2.31f, -8.9f},  // bench 2026-11-23, CM1A takes B/E
        {2.48f, -7.7f},  // bench 2026-11-23, CM1A takes B/E
        {2.65f, -6.7f},  // bench 2026-11-23, CM1A takes B/E
        {2.81f, -5.8f},  // bench 2026-11-23, CM1A takes B/E
        {2.98f, -5.0f},  // bench 2026-11-23, CM1A takes B/E
        {3.06f, -4.6f},  // bench 2026-11-23, CM1A takes B/E
        {3.48f, -3.1f},  // bench 2026-11-23, CM1A takes B/E
        {3.98f, -1.6f},  // bench 2026-11-23, CM1A takes B/E
        {4.48f, -0.5f},  // bench 2026-11-23, CM1A takes B/E
        {4.98f, 0.0f},  // bench 2026-11-23, CM1A takes B/E
        {5.31f, 0.0f},  // bench 2026-11-23, CM1A takes B/E
    }};

    float softClipV = 10.f;           // ASSUMPTION
    float softKneeV = 8.f;            // ASSUMPTION: transparent below this
    double knobSlewSeconds = 0.001;   // ASSUMPTION
};

class Vca902 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Params {
        bool exp = false;
        float fixedCv = 0.f;  // knob 0..1
    };

    struct Inputs {
        float in = 0.f;
        float in2 = 0.f;
        float cv[3] = {0.f, 0.f, 0.f};
    };

    struct Outputs {
        float out = 0.f;
        float outNeg = 0.f;
    };

    Vca902Calibration cal;

    // Steady-state gain for the knob position and summed external CV.
    float gain(const Params& p, float cvSum) const {
        return gainAtVoltage(p.exp, fixedCvVolts(p) + cvSum);
    }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        const float target = fixedCvVolts(p);
        if (!initialized_) {
            fixedCvV_ = target;
            initialized_ = true;
        } else {
            const double step = cal.fixedCvMaxV / (cal.knobSlewSeconds * sampleRate);
            fixedCvV_ += std::clamp(target - fixedCvV_, -step, step);
        }

        const float cvSum = in.cv[0] + in.cv[1] + in.cv[2];
        const float vc = float(fixedCvV_ + cvSum);
        const float signal = gainAtVoltage(p.exp, vc) * (in.in - in.in2);
        const float out = softClip(signal);
        return {out, -out};
    }

    // Unity below the knee; above it, a tanh shoulder with slope 1 at the knee, bounded by softClipV.
    float softClip(float s) const {
        const float a = std::fabs(s);
        if (a <= cal.softKneeV) return s;
        const float room = cal.softClipV - cal.softKneeV;
        return std::copysign(cal.softKneeV + room * std::tanh((a - cal.softKneeV) / room), s);
    }

private:
    float fixedCvVolts(const Params& p) const {
        return cal.fixedCvMaxV * std::clamp(p.fixedCv, 0.f, 1.f);
    }

    float linearGain(float vc) const {
        if (vc <= cal.linMuteV) return 0.f;

        const float gain = (vc - cal.linMuteV) * cal.linGainPerVolt;
        // The quadratic knee joins the straight line and ceiling with matching slopes.
        if (gain <= cal.linMax - cal.linKneeHalfWidth) return gain;
        const float room = cal.linMax + cal.linKneeHalfWidth - gain;
        if (room <= 0.f) return cal.linMax;
        return cal.linMax - room * room / (4.f * cal.linKneeHalfWidth);
    }

    float gainAtVoltage(bool exp, float vc) const {
        if (!exp)
            return linearGain(vc);

        if (vc <= cal.expMuteV)
            return 0.f;

        const auto& table = cal.expTable;
        const auto& first = table.front();
        if (vc < first.volts) {
            // Fade in linear gain from zero, matching the first dB segment's
            // slope at the join. The cubic is monotone for the calibrated points.
            const auto& next = table[1];
            const float width = first.volts - cal.expMuteV;
            const float t = (vc - cal.expMuteV) / width;
            const float slope = std::log(10.f) / 20.f * width *
                (next.decibels - first.decibels) / (next.volts - first.volts);
            return std::pow(10.f, first.decibels / 20.f) *
                (t * t * (3.f - 2.f * t + slope * (t - 1.f)));
        }

        const auto hi = std::upper_bound(table.begin(), table.end(), vc,
            [](float volts, const Vca902Calibration::ExpPoint& point) {
                return volts < point.volts;
            });
        if (hi == table.end()) return std::pow(10.f, table.back().decibels / 20.f);

        const auto& lo = *(hi - 1);
        const float t = (vc - lo.volts) / (hi->volts - lo.volts);
        const float dB = lo.decibels + t * (hi->decibels - lo.decibels);
        return std::pow(10.f, dB / 20.f);
    }

    double fixedCvV_ = 0.0;
    bool initialized_ = false;
};

}  // namespace k280
