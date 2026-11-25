// Trigger conventions shared by every module (real volts).
//
// V-trigger: a positive voltage pulse. Active above kOnV, released below kOffV (Schmitt).
// S-trigger: "switch trigger", active when the input is pulled to ground. In the emulator an
//   S-trigger output idles at kIdleV and drops to 0 V while active; an S-trigger input is active
//   only while a cable is connected and its voltage is below kOnV (released above kOffV).
//   As on the hardware, a normal gate patched into an S-trigger input acts inverted.
//
// Thresholds are assumptions until measured on the rack (handoff: calibration).
#pragma once

namespace k280 {

struct VTrigConv {
    static constexpr float kOnV = 2.5f;
    static constexpr float kOffV = 1.5f;
};

struct STrigConv {
    static constexpr float kOnV = 1.0f;   // below this: active
    static constexpr float kOffV = 2.0f;  // above this: released
    static constexpr float kIdleV = 5.0f; // what our S-trigger outputs send when inactive
};

// Schmitt detector that also reports when, within the last sample, the state changed
// (fraction 0..1 of the sample interval, by linear interpolation of the input).
class TriggerInput {
public:
    enum class Kind { V, S };
    explicit TriggerInput(Kind k = Kind::V) : kind_(k) {}

    // Returns true if the active state changed during this sample; crossing time in *frac.
    // An unpatched input is never active. When there is no previous connected sample to
    // interpolate from (first sample, cable just patched or unpatched), changes happen at frac 0.
    bool process(float v, bool connected, float* frac) {
        bool next = active_;
        float thr = 0.f;
        if (!connected) {
            next = false;
        } else if (kind_ == Kind::V) {
            if (!active_ && v >= VTrigConv::kOnV) { next = true; thr = VTrigConv::kOnV; }
            else if (active_ && v < VTrigConv::kOffV) { next = false; thr = VTrigConv::kOffV; }
        } else {
            if (!active_ && v <= STrigConv::kOnV) { next = true; thr = STrigConv::kOnV; }
            else if (active_ && v > STrigConv::kOffV) { next = false; thr = STrigConv::kOffV; }
        }
        bool changed = next != active_;
        if (changed) {
            float f = 0.f;
            float dv = v - prev_;
            if (connected && hasPrev_ && dv != 0.f) f = (thr - prev_) / dv;
            *frac = f < 0.f ? 0.f : (f > 1.f ? 1.f : f);
        }
        active_ = next;
        prev_ = v;
        hasPrev_ = connected;
        return changed;
    }
    bool active() const { return active_; }

private:
    Kind kind_;
    bool active_ = false;
    float prev_ = 0.f;
    bool hasPrev_ = false;
};

}  // namespace k280
