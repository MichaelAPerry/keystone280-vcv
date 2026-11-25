// Behringer Abacus. See sim/docs/m5-specs.md for the QSG and Maths-derived model.
// ASSUMPTION: the published 4.5 V EXP rise peak is a measurement artefact. All
// response settings reach the same 10 V peak.
// ASSUMPTION: CH3's asymmetric voltage normal is linear on each side of noon.
// ASSUMPTION: patched channel outputs leave the mix bus by default, and EOC
// holds high while idle.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Confidence.hpp"

namespace k280 {

struct AbacusCalibration {
    struct TimePoint {
        double knob;
        double seconds;
    };

    // Shared LIN taper with logarithmic interpolation between clock positions.
    std::array<TimePoint, 11> timeTable{{
        {0, .0005},
        {.1, .0009},  // bench 2026-11-22, test 15a (interpolated)
        {.2, .0016},  // bench 2026-11-22, test 15a
        {.3, .0042},  // bench 2026-11-22, test 15a
        {.4, .0144},  // bench 2026-11-22, test 15a
        {.5, .05},  // bench 2026-11-22, test 15a/15b
        {.6, .14},  // bench 2026-11-22, test 15a/15b
        {.7, .44},  // bench 2026-11-22, test 15a/15b
        {.8, 3.1},  // bench 2026-11-22, test 15a/15b
        {.9, 9},  // bench 2026-11-22, test 15b (interpolated)
        {1, 25},  // bench 2026-11-22, test 15b
    }};
    // QSG maxima in seconds. Use their exact ratios to LIN.
    double logRiseMaxSeconds = 540, logFallMaxSeconds = 1515;
    double expRiseMaxSeconds = 7, expFallMaxSeconds = 5;
    double minSeconds = .0005;         // ASSUMPTION: lower bound after time modulation
    float responseShapeBase = 4.f;     // ASSUMPTION: phase exponent base
    float singleCvVoltsPerThrow = 16;  // ASSUMPTION: RISE/FALL volts per knob throw
    float bothCvVoltsPerOctave = 1;    // ASSUMPTION: BOTH volts per time octave
    float hysteresisV = 1;            // ASSUMPTION: CYCLE release hysteresis
    double eocMinSeconds = .001;      // ASSUMPTION: minimum EOC pulse width
    float triggerV = 1.1f;  // bench 2026-11-22, test 14
    float triggerHysteresisV = .5f;  // bench 2026-11-22, test 14
    double triggerEdgeSeconds = .001;  // bench 2026-11-22, test 14 (estimated)
    float triggerEdgeV = 1.f;  // bench 2026-11-22, test 14 (estimated)
    float triggerEdgeRearmRatio = .5f;  // bench 2026-11-22, test 14 (estimated)
    float cycleV = 2.5f;
    float eorHighV = 8.7f;  // bench 2026-11-22, test 14
    float eocHighV = 9.8f;  // bench 2026-11-22, test 16
    float peakV = 10, signalMaxV = 10, timeCvMaxV = 8, sumLimitV = 10;
    float normal14V = 10, normal2V = 10, normal3NegativeV = 5, normal3PositiveV = 6;
};

class Abacus {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;
    enum class Stage { Idle, Rise, Fall };

    struct Params {
        float rise1 = 0, fall1 = 0, response1 = .5f;
        float rise4 = 0, fall4 = 0, response4 = .5f;
        bool cycleBtn1 = false, cycleBtn4 = false;
        float att1 = .5f, att2 = .5f, att3 = .5f, att4 = .5f;
        bool patchedOutLeavesBus = true;
    };

    struct Inputs {
        float in1 = 0, in2 = 0, in3 = 0, in4 = 0, trig1 = 0, trig4 = 0;
        float rise1 = 0, both1 = 0, fall1 = 0, cycle1 = 0;
        float rise4 = 0, both4 = 0, fall4 = 0, cycle4 = 0;
        bool in1Connected = false, in2Connected = false, in3Connected = false, in4Connected = false;
        bool trig1Connected = false, trig4Connected = false;
        bool rise1Connected = false, both1Connected = false, fall1Connected = false, cycle1Connected = false;
        bool rise4Connected = false, both4Connected = false, fall4Connected = false, cycle4Connected = false;
        bool outConnected[4]{};
    };

    struct Outputs {
        float out[4]{};
        float unity1 = 0, unity4 = 0, eor = 0, eoc = 0, orOut = 0, sum = 0, inv = 0;
    };

    AbacusCalibration cal;

    // Physical channel numbers (1 and 4).
    bool cycling(int ch) const { return ch == 1 ? ch1_.cycling() : ch == 4 && ch4_.cycling(); }
    // Patch persistence for the CYCLE buttons (the jacks are not state).
    bool cycleButtonOn(int ch) const { return ch == 1 ? ch1_.buttonOn : ch == 4 && ch4_.buttonOn; }
    void restoreCycleButton(int ch, bool on) {
        if (ch == 1) ch1_.buttonOn = on;
        if (ch == 4) ch4_.buttonOn = on;
    }
    Stage stage(int ch) const { return ch == 1 ? ch1_.stage : ch == 4 ? ch4_.stage : Stage::Idle; }

    float timeSeconds(float knob, float response, bool fall) const {
        return static_cast<float>(segmentTime(knob, response, fall, 0));
    }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        const double dt = 1.0 / sampleRate;
        Outputs o;
        o.unity1 = processChannel(ch1_, dt, p.rise1, p.fall1, p.response1, p.cycleBtn1,
            in.in1, in.in1Connected, in.trig1, in.trig1Connected, in.cycle1, in.cycle1Connected,
            {cv(in.rise1, in.rise1Connected), cv(in.fall1, in.fall1Connected), cv(in.both1, in.both1Connected)});
        o.unity4 = processChannel(ch4_, dt, p.rise4, p.fall4, p.response4, p.cycleBtn4,
            in.in4, in.in4Connected, in.trig4, in.trig4Connected, in.cycle4, in.cycle4Connected,
            {cv(in.rise4, in.rise4Connected), cv(in.fall4, in.fall4Connected), cv(in.both4, in.both4Connected)});
        o.out[0] = o.unity1 * attenuverter(p.att1);
        o.out[1] = (in.in2Connected ? in.in2 : cal.normal2V) * attenuverter(p.att2);
        const float att3 = attenuverter(p.att3);
        o.out[2] = (in.in3Connected ? in.in3 : (att3 < 0 ? cal.normal3NegativeV : cal.normal3PositiveV)) * att3;
        o.out[3] = o.unity4 * attenuverter(p.att4);

        o.eor = ch1_.stage == Stage::Fall ? cal.eorHighV : 0;
        o.eoc = ch4_.stage == Stage::Idle || ch4_.eocRemaining > 0 ? cal.eocHighV : 0;
        ch4_.eocRemaining = std::max(0.0, ch4_.eocRemaining - dt);

        const bool onBus[] = {in.in1Connected || ch1_.activeThisSample, true, true,
                              in.in4Connected || ch4_.activeThisSample};
        for (int i = 0; i < 4; ++i) {
            if (!onBus[i] || (p.patchedOutLeavesBus && in.outConnected[i])) continue;
            o.sum += o.out[i];
            o.orOut = std::max(o.orOut, o.out[i]);
        }
        o.sum = std::clamp(o.sum, -cal.sumLimitV, cal.sumLimitV);
        o.inv = -o.sum;
        return o;
    }

private:
    struct Comparator {
        bool high = false;
        bool wasConnected = false;
        float previous = 0;

        // Report a rising edge and its position within the sample interval.
        bool update(float volts, bool connected, float threshold, float hysteresis,
                    bool inclusive, double& fraction) {
            const bool above = inclusive ? volts >= threshold : volts > threshold;
            const bool next = connected && (high ? volts >= threshold - hysteresis : above);
            const bool rising = next && !high;
            if (rising) {
                fraction = wasConnected && volts != previous
                    ? std::clamp((double(threshold) - previous) / (double(volts) - previous), 0.0, 1.0)
                    : 0.0;
            }
            high = next;
            wasConnected = connected;
            previous = volts;
            return rising;
        }
    };

    struct Channel {
        Stage stage = Stage::Idle;
        double level = 0, start = 0, phase = 0, carrySeconds = 0, eocRemaining = 0;
        bool buttonOn = false, buttonWasDown = false, activeThisSample = false;
        Comparator cycle;
        bool trigArmed = true, trigWasConnected = false;
        float previousTrig = 0;
        double trigEdge = 0;
        double trigEdgeDecay = 0, trigEdgeRampGain = 0;
        double trigDecayExponent = 0;

        bool cycling() const { return buttonOn || cycle.high; }
        void startRise() {
            stage = Stage::Rise;
            start = level;
            phase = 0;
            carrySeconds = 0;
        }
    };

    struct TimeCv { float rise, fall, both; };

    static float attenuverter(float knob) { return 2 * std::clamp(knob, 0.f, 1.f) - 1; }

    float cv(float volts, bool connected) const {
        return connected ? std::clamp(volts, -cal.timeCvMaxV, cal.timeCvMaxV) : 0;
    }

    double linearTime(double knob) const {
        const auto& table = cal.timeTable;
        if (knob <= table.front().knob) return table.front().seconds;
        for (std::size_t i = 1; i < table.size(); ++i) {
            const auto& lo = table[i - 1];
            const auto& hi = table[i];
            if (knob > hi.knob) continue;
            const double fraction = (knob - lo.knob) / (hi.knob - lo.knob);
            return lo.seconds * std::pow(hi.seconds / lo.seconds, fraction);
        }
        return table.back().seconds;
    }

    double segmentTime(double knob, float response, bool fall, float both) const {
        const double r = std::clamp(response, 0.f, 1.f);
        const double linearMax = cal.timeTable.back().seconds;
        const double outerMax = r <= .5 ? (fall ? cal.logFallMaxSeconds : cal.logRiseMaxSeconds)
                                        : (fall ? cal.expFallMaxSeconds : cal.expRiseMaxSeconds);
        const double scale = std::pow(outerMax / linearMax, std::abs(2 * r - 1));
        return std::clamp(linearTime(knob) * scale * std::exp2(-both / cal.bothCvVoltsPerOctave),
                          cal.minSeconds, linearMax * scale);
    }

    void advance(Channel& state, double dt, double riseTime, double fallTime,
                 double exponent, float target) const {
        if (dt <= 0) return;
        if (state.stage == Stage::Idle) {
            const double delta = target - state.level;
            state.level += std::clamp(delta, -cal.peakV * dt / fallTime, cal.peakV * dt / riseTime);
            return;
        }

        state.activeThisSample = true;
        const bool rising = state.stage == Stage::Rise;
        const double duration = rising ? riseTime : fallTime;
        const double elapsed = dt + state.carrySeconds;
        state.carrySeconds = 0;
        const double remaining = (1 - state.phase) * duration;
        if (elapsed < remaining && state.phase + elapsed / duration < 1) {
            state.phase += elapsed / duration;
            const double shaped = std::pow(state.phase, exponent);
            state.level = rising ? state.start + (cal.peakV - state.start) * shaped
                                 : cal.peakV * (1 - shaped);
            return;
        }

        // Emit the exact endpoint on the crossing sample, preserving elapsed time
        // for the next segment so fractional samples don't slow the oscillator.
        state.phase = 0;
        state.level = rising ? cal.peakV : 0;
        if (rising) {
            state.stage = Stage::Fall;
        } else {
            state.eocRemaining = cal.eocMinSeconds;
            state.stage = Stage::Idle;
            if (state.cycling()) state.startRise();
        }
        if (state.stage != Stage::Idle) state.carrySeconds = std::max(0.0, elapsed - remaining);
    }

    float processChannel(Channel& state, double dt, float rise, float fall, float response,
                         bool button, float signal, bool signalConnected, float trig, bool trigConnected,
                         float cycle, bool cycleConnected, TimeCv modulation) const {
        if (button && !state.buttonWasDown) state.buttonOn = !state.buttonOn;
        state.buttonWasDown = button;
        double cycleFraction = 0;
        state.cycle.update(cycle, cycleConnected, cal.cycleV, cal.hysteresisV, true, cycleFraction);
        double trigFraction = 0;
        // Integrate the voltage change as a linear ramp over this sample interval.
        // The exponential decay and ramp gain keep qualification consistent across rates.
        const double decayExponent = dt / cal.triggerEdgeSeconds;
        if (decayExponent != state.trigDecayExponent) {
            state.trigEdgeDecay = std::exp(-decayExponent);
            state.trigEdgeRampGain = -std::expm1(-decayExponent) / decayExponent;
            state.trigDecayExponent = decayExponent;
        }
        if (!trigConnected) {
            state.trigEdge = 0;
            state.trigArmed = true;
        } else {
            const double change = double(trig) - state.previousTrig;
            state.trigEdge = std::max(0.0, state.trigEdgeDecay * state.trigEdge
                + state.trigEdgeRampGain * change);
            if (trig < cal.triggerV - cal.triggerHysteresisV ||
                state.trigEdge < cal.triggerEdgeV * cal.triggerEdgeRearmRatio)
                state.trigArmed = true;
        }
        const bool triggered = trigConnected && state.trigArmed && trig > cal.triggerV
            && state.trigEdge >= cal.triggerEdgeV;
        if (triggered) {
            state.trigArmed = false;
            if (state.trigWasConnected && state.previousTrig < cal.triggerV)
                trigFraction = (double(cal.triggerV) - state.previousTrig) / (double(trig) - state.previousTrig);
        }
        state.trigWasConnected = trigConnected;
        state.previousTrig = trigConnected ? trig : 0;

        const double riseTime = segmentTime(rise + modulation.rise / cal.singleCvVoltsPerThrow,
                                            response, false, modulation.both);
        const double fallTime = segmentTime(fall + modulation.fall / cal.singleCvVoltsPerThrow,
                                            response, true, modulation.both);
        const double exponent = std::pow(cal.responseShapeBase, 2 * std::clamp(response, 0.f, 1.f) - 1);
        const float target = signalConnected ? std::clamp(signal, -cal.signalMaxV, cal.signalMaxV) : 0;
        state.activeThisSample = state.stage != Stage::Idle;
        if (state.cycling() && state.stage == Stage::Idle) {
            const double fraction = state.buttonOn ? 0 : cycleFraction;
            advance(state, dt * fraction, riseTime, fallTime, exponent, target);
            state.startRise();
            dt *= 1 - fraction;
        } else if (triggered && !state.cycling() && state.stage != Stage::Rise) {
            advance(state, dt * trigFraction, riseTime, fallTime, exponent, target);
            state.startRise();
            dt *= 1 - trigFraction;
        }
        advance(state, dt, riseTime, fallTime, exponent, target);
        state.activeThisSample |= state.stage != Stage::Idle;

        // QSG (control 20/21): the channel attenuverter "does not pass internal voltage unless
        // processing is taking place", so an idle, unpatched channel outputs its 0 V baseline.
        // The +10 V normal is only the target the function rises to.
        return static_cast<float>(state.level);
    }

    Channel ch1_, ch4_;
};

}  // namespace k280
