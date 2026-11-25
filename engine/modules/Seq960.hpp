// Behringer 960 Sequential Controller. Model and QSG data: sim/docs/m3-specs.md.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Confidence.hpp"

namespace k280 {

struct Seq960Calibration {
    double hzLo[6] = {0.04, 2.75, 0.17, 11.0, 0.7, 44.0};
    double hzHi[6] = {0.5, 30.0, 2.0, 130.0, 8.0, 500.0};
    float cvLimitV = 5.f;         // ASSUMPTION: model clamp for the 1 V/oct input
    // rack 2026-11-22: a 921 RECT (-0.525 / +0.571 V, sub-audio) steps SHIFT, so the threshold is below
    // ~0.55 V; exact value unmeasured. Release just above 0 V so the rect's low half rearms it.
    float shiftOnV = 0.35f;
    float shiftHysteresisV = 0.3f;
    float stageOnV = 3.5f;
    float oscOnOffV = 3.5f;
    float hysteresisV = 1.f;     // ASSUMPTION: release this far below assertion
    float rowMaxV[3] = {2.f, 4.f, 8.f};  // ASSUMPTION, unmeasured; patch 1 suggests X2 is lower (bench 2026-11-23)
    float offStageRowV = 0.f;    // ASSUMPTION: stage 9 has no voltage knobs
    float stageOutV = 5.f;       // QSG level; ASSUMPTION: gate lasts the whole stage
    float oscOutV = 5.f;         // ASSUMPTION: QSG's +4 dBu is ambiguous for a pulse
    double oscDuty = 0.32;  // bench 2026-11-22, test 8
    double oscStartPhase = 0.0;  // ASSUMPTION: a full cycle before the first step
    struct RatePoint { double input; double factor; };
    std::array<RatePoint, 3> timingSlow{{  // Row C volts to relative rate
        {0.0, 1.0 / 2.1},  // bench 2026-11-22, test 11d
        {0.4, 0.74},       // bench 2026-11-22, test 11b
        {1.0, 1.0},        // bench 2026-11-22, test 11b
    }};
    std::array<RatePoint, 3> timingCeiling{{  // Base period to maximum relative rate
        {0.140, 1.20},  // bench 2026-11-22, test 11d
        {0.606, 1.31},  // bench 2026-11-22, test 11b
        {2.265, 2.01},  // bench 2026-11-22, test 11c
    }};
    std::array<RatePoint, 3> timingFoldback{{  // Row C volts to ceiling multiplier
        {2.0, 1.0},          // bench 2026-11-22, test 11b
        {4.0, 1.21 / 1.31},  // bench 2026-11-22, test 11b
        {8.0, 1.12 / 1.31},  // bench 2026-11-22, test 11b
    }};
};

class Seq960 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;
    enum class StopMode { OscOnly, BlockShift };

    struct Params {
        int freqRange = 0;
        float vernier = 0.5f;
        float knobs[3][8]{};
        int mode[8] = {1, 1, 1, 1, 1, 1, 1, 1};  // SKIP / NORMAL / STOP
        int mode9 = 0;                            // SKIP / STOP
        int rowRange[3]{};                        // X1 / X2 / X4
        bool timing = false;
        StopMode stopMode = StopMode::BlockShift;  // rack 2026-11-22: external SHIFT does not leave a STOP stage
    };

    struct Inputs {
        bool oscOn = false, oscOff = false, shift = false, set[9]{};
        float oscOnV = 0.f, oscOffV = 0.f, shiftV = 0.f, oscCv = 0.f, sin[9]{};
        bool oscOnConnected = false, oscOffConnected = false, shiftConnected = false;
        bool oscCvConnected = false, sinConnected[9]{};
    };

    struct Outputs {
        float oscOut = 0.f;
        float sout[9]{};
        float row[3]{};
    };

    Seq960Calibration cal;

    int stage() const { return stage_; }
    bool oscRunning() const { return running_; }
    int saveStage() const { return stage_; }

    void restoreStage(int stage) {
        reset();
        stage_ = std::clamp(stage, 1, 9);
    }

    // Patch persistence: whether the oscillator was running. Restarting resets its phase.
    void restoreRunning(bool running) {
        running_ = running && stage_ != 9;
        phase_ = cal.oscStartPhase;
    }

    void reset() {
        const auto calibration = cal;
        *this = Seq960{};
        cal = calibration;
    }

    float oscHz(const Params& p, float cv) const {
        int range = std::clamp(p.freqRange, 0, 5);
        return float(cal.hzLo[range] *
                     std::pow(cal.hzHi[range] / cal.hzLo[range], knob(p.vernier)) *
                     std::exp2(std::clamp(cv, -cal.cvLimitV, cal.cvLimitV)));
    }

    double stageHz(const Params& p, int stage, double baseHz) const {
        if (baseHz <= 0) return 0;
        if (!p.timing || stage == 9) return baseHz;
        const double volts = knob(p.knobs[2][stage - 1]) *
            cal.rowMaxV[std::clamp(p.rowRange[2], 0, 2)];
        const double noonV = cal.timingSlow.back().input;
        if (volts <= noonV) return baseHz * interpolate(cal.timingSlow, volts);

        // Above X1 noon, the rate doubles per volt until it hits a base-period-dependent
        // ceiling. Above X1 full scale, the ceiling folds back as row voltage increases.
        const double ceiling = interpolate(cal.timingCeiling, 1.0 / baseHz) *
            interpolate(cal.timingFoldback, volts);
        return baseHz * std::min(std::exp2(volts - noonV), ceiling);  // bench 2026-11-22, test 11c
    }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        bool onJack = onJack_.rise(in.oscOnV, in.oscOnConnected, cal.oscOnOffV, cal.hysteresisV);
        bool offJack = offJack_.rise(in.oscOffV, in.oscOffConnected, cal.oscOnOffV, cal.hysteresisV);
        bool shiftJack = shiftJack_.rise(in.shiftV, in.shiftConnected, cal.shiftOnV, cal.shiftHysteresisV);
        bool on = buttonEdge(in.oscOn, onButton_) || onJack;
        bool off = buttonEdge(in.oscOff, offButton_) || offJack;
        bool shift = buttonEdge(in.shift, shiftButton_) || shiftJack;
        int selected = 0;
        for (int i = 0; i < 9; ++i) {
            bool set = buttonEdge(in.set[i], setButton_[i]);
            bool trigger = stageJack_[i].rise(in.sin[i], in.sinConnected[i],
                                              cal.stageOnV, cal.hysteresisV);
            if (!selected && (set || trigger)) selected = i + 1;
        }

        // Direct selection takes priority over SHIFT. OFF wins over ON when both arrive.
        if (on && !off) {
            running_ = true;
            phase_ = cal.oscStartPhase;
        }
        if (off) running_ = false;
        if (selected) {
            enter(selected, p);
        } else if (shift && !(p.stopMode == StopMode::BlockShift && isStop(stage_, p))) {
            advance(p);
        }

        // The start sample renders phase zero. The first wrap occurs a full period later.
        if (running_ && !on && sampleRate > 0.f) {
            clock(p, oscHz(p, in.oscCvConnected ? in.oscCv : 0.f), 1.0 / sampleRate);
        }

        Outputs out;
        if (running_ && phase_ < cal.oscDuty) out.oscOut = cal.oscOutV;
        out.sout[stage_ - 1] = cal.stageOutV;
        for (int row = 0; row < 3; ++row) {
            out.row[row] = stage_ == 9 ? cal.offStageRowV :
                knob(p.knobs[row][stage_ - 1]) * cal.rowMaxV[std::clamp(p.rowRange[row], 0, 2)];
        }
        return out;
    }

private:
    struct Comparator {
        bool high = false;

        bool rise(float volts, bool connected, float threshold, float hysteresis) {
            bool wasHigh = high;
            high = connected && (wasHigh ? volts >= threshold - hysteresis : volts >= threshold);
            return high && !wasHigh;
        }
    };

    static bool buttonEdge(bool pressed, bool& previous) {
        bool rise = pressed && !previous;
        previous = pressed;
        return rise;
    }

    static double interpolate(const std::array<Seq960Calibration::RatePoint, 3>& points,
                              double input) {
        if (input <= points.front().input) return points.front().factor;
        for (std::size_t i = 1; i < points.size(); ++i) {
            const auto& lo = points[i - 1];
            const auto& hi = points[i];
            if (input < hi.input)
                return lo.factor + (hi.factor - lo.factor) * (input - lo.input) /
                    (hi.input - lo.input);
        }
        return points.back().factor;
    }

    static float knob(float value) { return std::clamp(value, 0.f, 1.f); }

    static bool isStop(int stage, const Params& p) {
        return stage == 9 ? p.mode9 == 1 : p.mode[stage - 1] == 2;
    }

    void enter(int stage, const Params& p) {
        stage_ = stage;
        if (stage == 9 || isStop(stage, p)) running_ = false;
    }

    void advance(const Params& p) {
        int next = stage_;
        for (int i = 0; i < 8; ++i) {
            if (next == 8 && p.mode9 == 1) break;
            next = next >= 8 ? 1 : next + 1;
            if (p.mode[next - 1] != 0) {
                enter(next, p);
                return;
            }
        }
        enter(9, p);
    }

    void clock(const Params& p, double hz, double seconds) {
        // Carry the remaining sample time into the next stage at its own rate.
        while (running_ && seconds > 0.0) {
            const double rate = stageHz(p, stage_, hz);
            double cycles = seconds * rate;
            double toWrap = 1.0 - phase_;
            if (cycles < toWrap) {
                phase_ += cycles;
                return;
            }
            seconds -= toWrap / rate;
            phase_ = 0.0;
            advance(p);
        }
    }

    int stage_ = 1;
    bool running_ = false;
    double phase_ = 0.0;
    bool onButton_ = false, offButton_ = false, shiftButton_ = false, setButton_[9]{};
    Comparator onJack_, offJack_, shiftJack_, stageJack_[9];
};

}  // namespace k280
