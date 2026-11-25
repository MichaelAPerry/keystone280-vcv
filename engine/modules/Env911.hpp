// Behringer 911 Envelope Generator. See sim/docs/m3-specs.md for the panel taper and RC model.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Confidence.hpp"
#include "models/Trigger.hpp"

namespace k280 {

struct Env911Calibration {
    struct TimePoint {
        float knob;
        float seconds;
    };

    // T1 seconds to peak. Minimum from test 8. The other settings are an
    // unmeasured ASSUMPTION carried over from the previous taper.
    std::array<TimePoint, 5> attackTable{{
        {0.0f, 0.002f},
        {0.2f, 0.47f},
        {0.5f, 1.4f},
        {0.8f, 15.f},
        {1.0f, 30.f},
    }};

    // T2 and T3 seconds to 10% of the excursion (one table: T2 noon and T3 noon
    // both measured 1.2 s, bench 2026-11-23 envshots_long_171204 / _172048).
    std::array<TimePoint, 5> timeTable{{
        {0.0f, 0.018f},   // bench 2026-11-23, patch 1 live take and test 8
        {0.2f, 0.5f},     // bench 2026-11-23, envshots_long_170837/170923
        {0.5f, 1.2f},     // bench 2026-11-23, envshots_long_171204
        // Near the lower bound implied by a 6 V peak and a >=4.94 V tail at 1.5 s.
        {0.8f, 18.f},     // bench 2026-11-23, envshots_long_171617
        {1.0f, 29.f},
    }};

    // Sustain CV from cutoff ratios relative to rest. The 3 o'clock point
    // extrapolates the measurable RC release back to gate-off.
    struct SustainPoint {
        float knob;
        float volts;
    };
    std::array<SustainPoint, 4> sustainTable{{
        {0.f, 0.f},
        {0.5f, 3.f},      // bench 2026-11-23, envshots_long_171801
        {0.8f, 5.55f},    // bench 2026-11-23, envshots_long_171930/172048
        {1.f, 6.f},       // bench 2026-11-23, envshots_long_172048 (assumed peak limit)
    }};

    float kPeakV = 6.f;  // ASSUMPTION

    // ASSUMPTION: RC attack target, fraction remaining after T2/T3, and release cutoff.
    float attackTargetRatio = 1.5f;
    float remainingFraction = 0.1f;
    float releaseCutoffV = 0.001f;
};

class Env911 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    struct Params {
        float t1 = 0.f;
        float t2 = 0.f;
        float esus = 0.f;
        float t3 = 0.f;
    };

    struct Inputs {
        float strig = 0.f;
        bool strigConnected = false;
    };

    struct Outputs {
        float out = 0.f;
    };

    Env911Calibration cal;

    float attackSeconds(float knob) const { return interpolateTime(cal.attackTable, knob); }
    float timeSeconds(float knob) const { return interpolateTime(cal.timeTable, knob); }
    float releaseSeconds(float knob) const { return timeSeconds(knob); }

    float sustainVolts(float knob) const {
        const auto& table = cal.sustainTable;
        knob = std::clamp(knob, table.front().knob, table.back().knob);
        const auto hi = std::lower_bound(table.begin() + 1, table.end(), knob,
            [](const auto& point, float value) { return point.knob < value; });
        const auto& lo = *(hi - 1);
        return lo.volts + (hi->volts - lo.volts) * ((knob - lo.knob) / (hi->knob - lo.knob));
    }

    Stage stage() const { return stage_; }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        const double dt = 1.0 / sampleRate;
        float fraction = 0.f;
        if (trigger_.process(in.strig, in.strigConnected, &fraction)) {
            advance(dt * fraction, p);
            stage_ = trigger_.active() ? Stage::Attack : Stage::Release;
        }
        advance(dt * (1.0 - fraction), p);
        return {static_cast<float>(volts_)};
    }

private:
    static float interpolateTime(const std::array<Env911Calibration::TimePoint, 5>& table, float knob) {
        if (knob <= table.front().knob) return table.front().seconds;
        for (std::size_t i = 1; i < table.size(); ++i) {
            const auto& lo = table[i - 1];
            const auto& hi = table[i];
            if (knob == hi.knob) return hi.seconds;
            if (knob < hi.knob) {
                const float fraction = (knob - lo.knob) / (hi.knob - lo.knob);
                return lo.seconds * std::pow(hi.seconds / lo.seconds, fraction);
            }
        }
        return table.back().seconds;
    }

    void advance(double dt, const Params& p) {
        if (dt <= 0.0) return;
        switch (stage_) {
            case Stage::Idle:
                break;
            case Stage::Attack: {
                const double peak = cal.kPeakV;
                const double target = peak * cal.attackTargetRatio;
                const double rate = std::log(target / (target - peak)) / attackSeconds(p.t1);
                volts_ += (target - volts_) * -std::expm1(-rate * dt);
                if (volts_ >= peak) {
                    // Emit the exact peak on the sample that reaches it.
                    volts_ = peak;
                    stage_ = Stage::Decay;
                }
                break;
            }
            case Stage::Decay: {
                const double sustain = sustainVolts(p.esus);
                volts_ = sustain + (volts_ - sustain) *
                    std::pow(double(cal.remainingFraction), dt / timeSeconds(p.t2));
                // An RC curve approaches sustain asymptotically. Use the release resolution
                // for the transition to a steady sustain level as well.
                if (std::abs(volts_ - sustain) < cal.releaseCutoffV) {
                    volts_ = sustain;
                    stage_ = Stage::Sustain;
                }
                break;
            }
            case Stage::Sustain:
                volts_ = sustainVolts(p.esus);
                break;
            case Stage::Release:
                volts_ *= std::pow(double(cal.remainingFraction), dt / releaseSeconds(p.t3));
                if (volts_ < cal.releaseCutoffV) {
                    volts_ = 0.0;
                    stage_ = Stage::Idle;
                }
                break;
        }
    }

    Stage stage_ = Stage::Idle;
    double volts_ = 0.0;
    TriggerInput trigger_{TriggerInput::Kind::S};
};

}  // namespace k280
