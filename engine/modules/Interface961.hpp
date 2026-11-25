// Behringer 961 Interface. Source: Quick Start Guide and sim/docs/m3-specs.md.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

#include "Confidence.hpp"
#include "models/Trigger.hpp"

namespace k280 {

struct Interface961Calibration {
    float vTrigOnV = 1.5f;    // QSG minimum trigger level
    float vTrigOffV = 1.f;   // ASSUMPTION: Schmitt release threshold
    // Both banks use the same switch-on law, with logarithmic interpolation.
    struct TimePoint { float knob; float seconds; };
    std::array<TimePoint, 4> switchOnTable{{
        {0.0f, 0.045f},  // bench 2026-11-22, test 8
        {0.5f, 0.275f},  // bench 2026-11-22, test 8
        {0.8f, 3.0f},    // bench 2026-11-22, test 8
        {1.0f, 4.2f},    // bench 2026-11-22, test 8
    }};
    float audioHighPassHz = 10.f;
    float minAudioGain = 0.5f;
    float maxAudioGain = 100.f;
    // Model fit for tests 8b/8c with the calibrated 904A in the signal path.
    float audioOnV = 0.88f;
    float audioOffV = 0.65f;
    double audioSettleSeconds = 0.020;
    double audioAttackSeconds = 0.002;
    double audioReleaseSeconds = 0.006;
    float audioGateV = 5.f;
};

class Interface961 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Params {
        float sens = 0.f;
        float time[2] = {};
    };

    // All six jacks in each column share the same behavior.  // bench 2026-11-22, test 9
    static constexpr int kJacksPerColumn = 6;

    struct BankInputs {
        float sIn = 0.f;
        bool sInConnected = false;
        float a[kJacksPerColumn] = {};
        bool aConnected[kJacksPerColumn] = {};
        float b[kJacksPerColumn] = {};
        bool bConnected[kJacksPerColumn] = {};
    };

    struct Inputs {
        float audio = 0.f;
        bool audioConnected = false;
        BankInputs bank[2]{};
    };

    struct BankOutputs {
        float sv = 0.f;
        float sOut = 12.f;
    };

    struct Outputs {
        float avt = 0.f;
        BankOutputs bank[2];
    };

    Interface961Calibration cal;

    float switchOnSeconds(float knob) const {
        const auto& t = cal.switchOnTable;
        if (knob <= t.front().knob) return t.front().seconds;
        for (std::size_t i = 1; i < t.size(); ++i) {
            if (knob < t[i].knob) {
                const float f = (knob - t[i - 1].knob) / (t[i].knob - t[i - 1].knob);
                return t[i - 1].seconds * std::pow(t[i].seconds / t[i - 1].seconds, f);
            }
        }
        return t.back().seconds;
    }

    bool bankActive(int b) const { return banks_[b].active; }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        const double dt = 1.0 / sampleRate;
        Outputs out;
        out.avt = processAudio(dt, p.sens, in.audio, in.audioConnected);
        for (int b = 0; b < 2; ++b) {
            auto& state = banks_[b];
            const auto& input = in.bank[b];
            float frac = 0.f;
            state.sIn.process(input.sIn, input.sInConnected, &frac);
            out.bank[b].sv = state.sIn.active() ? 5.f : 0.f;

            bool anyA = false;
            bool bRise = false;
            for (int j = 0; j < kJacksPerColumn; ++j) {
                state.a[j].process(input.a[j], input.aConnected[j], cal);
                anyA |= state.a[j].high;
                // Track every input even during a pulse, so ignored edges cannot fire later.
                bRise |= state.b[j].process(input.b[j], input.bConnected[j], cal);
            }

            if (bRise && !anyA && state.remaining <= 0.0) {
                state.remaining = switchOnSeconds(p.time[b]);
            }
            state.active = anyA || state.remaining > 0.0;
            out.bank[b].sOut = state.active ? 0.f : 12.f;
            state.remaining = std::max(0.0, state.remaining - dt);
        }
        return out;
    }

private:
    struct VTrigInput {
        bool high = false;

        bool process(float volts, bool connected, const Interface961Calibration& cal) {
            const bool wasHigh = high;
            high = connected && (wasHigh ? volts >= cal.vTrigOffV : volts >= cal.vTrigOnV);
            return high && !wasHigh;
        }
    };

    struct BankState {
        TriggerInput sIn{TriggerInput::Kind::S};
        VTrigInput a[kJacksPerColumn], b[kJacksPerColumn];
        double remaining = 0.0;
        bool active = false;
    };

    float processAudio(double dt, float sens, float audio, bool connected) {
        if (!connected) {
            audioDc_ = audioEnvelope_ = 0.0;
            audioConnectedSeconds_ = 0.0;
            audioHigh_ = false;
            audioArmed_ = false;
            return 0.f;
        }
        constexpr double twoPi = 6.283185307179586;
        const double coupling = -std::expm1(-twoPi * cal.audioHighPassHz * dt);
        const double ac = audio - audioDc_;
        audioDc_ += coupling * ac;
        const double gain = cal.minAudioGain *
            std::pow(cal.maxAudioGain / cal.minAudioGain, std::clamp(sens, 0.f, 1.f));
        const double rectified = std::fabs(ac * (1.0 - coupling) * gain);
        const double time = rectified > audioEnvelope_ ? cal.audioAttackSeconds :
                                                        cal.audioReleaseSeconds;
        audioEnvelope_ += -std::expm1(-dt / time) * (rectified - audioEnvelope_);
        audioHigh_ = audioHigh_ ? audioEnvelope_ >= cal.audioOffV : audioEnvelope_ > cal.audioOnV;
        // An already-present signal can hold the detector high at patch-in.
        // Wait for a release before treating a subsequent attack as a trigger.
        audioConnectedSeconds_ = std::min(cal.audioSettleSeconds, audioConnectedSeconds_ + dt);
        if (audioConnectedSeconds_ >= cal.audioSettleSeconds && !audioHigh_) audioArmed_ = true;
        return audioArmed_ && audioHigh_ ? cal.audioGateV : 0.f;
    }

    BankState banks_[2];
    double audioDc_ = 0.0;
    double audioEnvelope_ = 0.0;
    double audioConnectedSeconds_ = 0.0;
    bool audioHigh_ = false;
    bool audioArmed_ = false;
};

}  // namespace k280
