// Behringer 962 Sequential Switch. Model and QSG data: sim/docs/m4-specs.md.
#pragma once

#include <algorithm>

#include "Confidence.hpp"

namespace k280 {

struct Seq962Calibration {
    float shiftOnV = 1.5f;
    float tinOnV = 3.5f;
    float hysteresisV = 1.f;  // ASSUMPTION: release this far below assertion
    float toutOnV = 5.f;     // QSG level; gate lasts the whole stage
};

class Seq962 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Inputs {
        float shiftV = 0.f, tin[3]{}, sin[3]{};
        bool shiftConnected = false, tinConnected[3]{}, sinConnected[3]{};
        bool btn[3]{};
    };

    struct Outputs {
        float out = 0.f;
        float tout[3]{};
    };

    Seq962Calibration cal;

    int stage() const { return stage_; }
    int saveStage() const { return stage_; }

    void restoreStage(int stage) {
        reset();
        stage_ = std::clamp(stage, 1, 3);
    }

    void reset() {
        const auto calibration = cal;
        *this = Seq962{};
        cal = calibration;
    }

    Outputs process(float /* sampleRate */, const Inputs& in) {
        bool shift = shiftJack_.rise(in.shiftV, in.shiftConnected, cal.shiftOnV, cal.hysteresisV);
        int selected = 0;
        for (int i = 0; i < 3; ++i) {
            bool button = in.btn[i] && !previousButton_[i];
            previousButton_[i] = in.btn[i];
            bool trigger = tinJack_[i].rise(in.tin[i], in.tinConnected[i], cal.tinOnV, cal.hysteresisV);
            if (!selected && (button || trigger)) selected = i + 1;
        }

        // Direct selection wins over SHIFT, and the lowest stage wins between simultaneous selects.
        if (selected) {
            stage_ = selected;  // Confirmed on the rack 2026-11-22: buttons select an unpatched stage 3; SHIFT is 2-way unless SIG IN 3 is patched.
        } else if (shift) {
            int lastStage = in.sinConnected[2] ? 3 : 2;
            stage_ = stage_ >= lastStage ? 1 : stage_ + 1;
        }

        int active = stage_ - 1;
        Outputs out;
        // ASSUMPTION: switching is instant.
        out.out = in.sinConnected[active] ? in.sin[active] : 0.f;
        out.tout[active] = cal.toutOnV;
        return out;
    }

private:
    struct Comparator {
        bool high = false;

        bool rise(float volts, bool connected, float threshold, float hysteresis) {
            bool wasHigh = high;
            high = connected && volts >= (wasHigh ? threshold - hysteresis : threshold);
            return high && !wasHigh;
        }
    };

    int stage_ = 1;
    bool previousButton_[3]{};
    Comparator shiftJack_, tinJack_[3];
};

}  // namespace k280
