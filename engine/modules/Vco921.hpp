// Behringer 921 Voltage Controlled Oscillator (clone of the Moog 921).
//
// Source: Behringer 921 Quick Start Guide (controls, specifications, calibration procedure).
// No circuit model yet, so confidence is "manual". Numbers the guide does not give are marked
// ASSUMPTION and live in Vco921Calibration, to be replaced by measurements of Michael's units.
//
// From the guide:
//   RANGE 32' 16' 8' 4' 2' 1' in one-octave steps; FREQUENCY knob ±6 octaves, or ±12 semitones
//   with the SCALE switch; COARSE RNG audio (about 1 Hz to 80 kHz) or sub-audio (about 0.01 Hz
//   to 800 Hz, i.e. the audio range divided by 100); 3 summed 1 V/oct inputs (±6 V);
//   rectangular width 10–90 %, 2 summed CV inputs at 1 V per 15 %; clamping point 10–90 %,
//   "the point at which the oscillator waveform resets", triggered by V-trig or S-trig;
//   waveform outputs typically −4 dBu; aux output up to +10 dBu with six selectable waveforms
//   (sine, triangle, inverted saw, saw, rectangle, inverted rectangle) and ± outputs.
#pragma once
#include <algorithm>
#include <cmath>

#include "Confidence.hpp"
#include "models/Blep.hpp"
#include "models/Trigger.hpp"

namespace k280 {

struct Vco921Calibration {
    // ASSUMPTION: 8' with FREQUENCY at noon and no CV is C4. Consistent with the guide's
    // "audio range typically 1 Hz": 32' with FREQUENCY fully down is 261.6 / 4 / 64 = 1.02 Hz.
    float hzAt8ftCentre = 261.6256f;
    static constexpr float unitBOffsetSemitones = -1.85f;  // bench 2026-11-21, test 1
    float unitOffsetSemitones = 0.f;
    float voltsPerOctave = 1.0f;     // trimmed to 1 V/oct (guide's SCALE and HI ADJ trimmers)
    float audioMaxHz = 80000.f;      // guide: typically 80 kHz
    float subAudioDivisor = 100.f;   // guide: sub-audio 0.01–800 Hz vs audio 1 Hz–80 kHz
    float sinePeakV = 0.745f;  // bench 2026-11-21, Test 20
    float triPeakV = 0.811f;   // bench 2026-11-21, Test 20
    // ASSUMPTION: the waveform shapes measured near 224 Hz apply at all pitches.
    static constexpr double triRiseShare = 0.5114;  // bench 2026-11-21, Test 20
    static constexpr double triQuadratic = 0.02281; // bench 2026-11-21, Test 20
    // Chebyshev coefficients relative to the sine fundamental.
    static constexpr float sineHarmonics[] = {
         0.016982f,  // H2, bench 2026-11-21, Test 20
         0.018408f,  // H3, bench 2026-11-21, Test 20
        -0.001862f,  // H4, bench 2026-11-21, Test 20
        -0.006839f,  // H5, bench 2026-11-21, Test 20
         0.000891f,  // H6, bench 2026-11-21, Test 20
        -0.003673f,  // H7, bench 2026-11-21, Test 20
        -0.000631f,  // H8, bench 2026-11-21, Test 20
    };
    float sawPeakV = 1.10f;   // bench 2026-11-21, test 20
    // Model the measured rectangle swing centered at ground.
    float rectPeakV = 0.55f;  // bench 2026-11-21, test 20
    // Guide: aux "Off to +10 dBu". ASSUMPTION: linear knob, +10 dBu sine peak at full.
    float auxMaxPeakV = 3.4650f;
};

inline constexpr float kHalfPi = 1.57079632679f;

class Vco921 {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    enum Range { R32, R16, R8, R4, R2, R1, NUM_RANGES };
    enum AuxWave { AUX_SINE, AUX_TRI, AUX_INV_SAW, AUX_SAW, AUX_RECT, AUX_INV_RECT, NUM_AUX };

    // What a clamp trigger does. The guide says the clamping point is where the waveform
    // "resets"; the Moog name "clamp" suggests the core is held there while the trigger lasts.
    // Which one the Behringer does is for the rack to settle (handoff: patch 23).
    enum class ClampMode { Hold, Reset };

    struct Params {
        float frequency = 0.5f;   // knob 0..1 (7 o'clock .. 5 o'clock)
        int range = R8;
        bool semitoneScale = false;  // SCALE switch up: ±12 SEMI; down: ±6 OCT
        bool subAudio = false;       // COARSE RNG up: SUB; down: AUDIO
        float width = 0.5f;       // RECTANGULAR WIDTH knob 0..1 -> 10..90 %
        float clampPoint = 0.5f;  // CLAMPING POINT knob 0..1 -> 10..90 %
        int auxWave = AUX_SINE;
        float auxLevel = 0.f;     // knob 0..1 -> off .. +10 dBu
        ClampMode clampMode = ClampMode::Hold;
    };

    struct Inputs {
        float fcv[3] = {0.f, 0.f, 0.f};  // FCV1..3, volts
        float pw[2] = {0.f, 0.f};        // PW1..2, volts
        float vtrig = 0.f;
        bool vtrigConnected = false;
        float strig = 0.f;
        bool strigConnected = false;
    };

    struct Outputs {
        float sine = 0.f, tri = 0.f, saw = 0.f, rect = 0.f, aux = 0.f, auxNeg = 0.f;
    };

    Vco921Calibration cal;

    // Frequency in Hz for the given panel state and summed 1 V/oct input.
    float frequencyHz(const Params& p, float cvSum) const {
        float knobOct = p.semitoneScale ? (p.frequency - 0.5f) * 2.f : (p.frequency - 0.5f) * 12.f;
        float rangeOct = float(std::clamp(p.range, 0, NUM_RANGES - 1) - R8);
        float hz = cal.hzAt8ftCentre * std::exp2(rangeOct + knobOct + cal.unitOffsetSemitones / 12.f +
                                               cvSum / cal.voltsPerOctave);
        float maxHz = cal.audioMaxHz;
        if (p.subAudio) {
            hz /= cal.subAudioDivisor;
            maxHz /= cal.subAudioDivisor;
        }
        return std::min(hz, maxHz);
    }

    static float pulseWidth(const Params& p, float cvSum) {
        return std::clamp(0.1f + 0.8f * p.width + 0.15f * cvSum, 0.1f, 0.9f);
    }

    // One sample. Outputs are delayed by one sample (band-limiting).
    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        float cv = in.fcv[0] + in.fcv[1] + in.fcv[2];
        // Rendered frequency is kept below Nyquist; above it the real module is inaudible anyway.
        double hz = std::min(frequencyHz(p, cv), 0.45f * sampleRate);
        double inc = hz / sampleRate;  // double: 0.01 Hz at 48 kHz is ~2e-7 per sample
        double clampPhase = 0.1 + 0.8 * std::clamp(p.clampPoint, 0.f, 1.f);

        // A width change can flip the rectangle with no edge crossing: band-limit that step too.
        float oldRect = rect(phase_);
        width_ = pulseWidth(p, in.pw[0] + in.pw[1]);
        if (rect(phase_) != oldRect) rectBlep_.addStep(rect(phase_) - oldRect, 0.f);

        // Clamp trigger: active while either input is active. Order the two inputs' changes in
        // time and keep every change of the combined state.
        struct Change { float frac; bool v; bool on; } ch[2];
        int nch = 0;
        bool vWas = vtrig_.active(), sWas = strig_.active();
        float f = 0.f;
        if (vtrig_.process(in.vtrig, in.vtrigConnected, &f)) ch[nch++] = {f, true, vtrig_.active()};
        if (strig_.process(in.strig, in.strigConnected, &f)) ch[nch++] = {f, false, strig_.active()};
        if (nch == 2 && ch[1].frac < ch[0].frac) std::swap(ch[0], ch[1]);
        struct ClampEvent { float frac; bool active; } events[2];
        int nev = 0;
        {
            bool v = vWas, sOn = sWas;
            for (int i = 0; i < nch; ++i) {
                (ch[i].v ? v : sOn) = ch[i].on;
                bool agg = v || sOn;
                if (nev && events[nev - 1].frac == ch[i].frac) {
                    // Same instant as the previous change: keep only the net result.
                    events[nev - 1].active = agg;
                    bool before = nev > 1 ? events[nev - 2].active : clampActive_;
                    if (agg == before) --nev;
                } else if (agg != (nev ? events[nev - 1].active : clampActive_)) {
                    events[nev++] = {ch[i].frac, agg};
                }
            }
        }
        int nextEv = 0;

        // Walk the sample interval event by event. Edges and wraps that land exactly on the end of
        // the interval are processed here (frac = 1), so no step escapes the BLEP. A clamp that
        // ties with an edge or wrap is processed just after it, at the same time.
        double t = 0.0;
        while (true) {
            double tNext = 1.0;
            enum { NONE, WRAP, EDGE, CLAMP } ev = NONE;
            if (running_ && inc > 0.0) {
                if (phase_ < width_) {
                    double te = t + (width_ - phase_) / inc;
                    if (te <= tNext) { tNext = te; ev = EDGE; }
                }
                double tw = t + (1.0 - phase_) / inc;
                if (tw < tNext || (tw <= tNext && ev == NONE)) { tNext = tw; ev = WRAP; }
            }
            if (nextEv < nev) {
                double tc = std::max(double(events[nextEv].frac), t);
                if (tc < tNext || (ev == NONE && tc <= tNext)) { tNext = tc; ev = CLAMP; }
            }
            if (running_) phase_ += inc * (tNext - t);
            t = tNext;
            if (ev == NONE) break;
            float ft = float(t);
            switch (ev) {
                case WRAP:
                    phase_ = 0.0;
                    sawBlep_.addStep(2.f, ft);  // falling saw resets from -1 to +1
                    rectBlep_.addStep(2.f, ft);  // width < 1, so the rectangle was low
                    break;
                case EDGE:
                    phase_ = width_;
                    rectBlep_.addStep(-2.f, ft);
                    break;
                case CLAMP: {
                    bool on = events[nextEv++].active;
                    clampActive_ = on;
                    if (on) {
                        jumpTo(clampPhase, ft);
                        if (p.clampMode == ClampMode::Hold) running_ = false;
                    } else {
                        running_ = true;  // Hold: resume from the clamping point
                    }
                    break;
                }
                default: break;
            }
        }
        if (phase_ >= 1.0) phase_ -= std::floor(phase_);

        const float sawNorm = sawBlep_.emit(saw(phase_));
        const float rectNorm = rectBlep_.emit(rect(phase_));
        const float triNorm = triBlep_.emit(tri(phase_));
        const float sineNorm = sineBlep_.emit(sine(phase_));
        Outputs o;
        o.saw = cal.sawPeakV * sawNorm;
        o.rect = cal.rectPeakV * rectNorm;
        o.tri = cal.triPeakV * triNorm;
        o.sine = cal.sinePeakV * sineNorm;
        float auxNorm = 0.f;
        switch (p.auxWave) {
            case AUX_SINE: auxNorm = sineNorm; break;
            case AUX_TRI: auxNorm = triNorm; break;
            case AUX_INV_SAW: auxNorm = -sawNorm; break;
            case AUX_SAW: auxNorm = sawNorm; break;
            case AUX_RECT: auxNorm = rectNorm; break;
            case AUX_INV_RECT: auxNorm = -rectNorm; break;
            default: break;
        }
        o.aux = auxNorm * cal.auxMaxPeakV * std::clamp(p.auxLevel, 0.f, 1.f);
        o.auxNeg = -o.aux;
        return o;
    }

    double phase() const { return phase_; }
    bool clamped() const { return clampActive_; }
    bool running() const { return running_; }

    // Waveforms of the core phase (0..1), with unit half-swing. Saw falls.
    static float saw(double ph) { return float(1.0 - 2.0 * ph); }
    float rect(double ph) const { return ph < width_ ? 1.f : -1.f; }
    static float tri(double ph) {
        constexpr double rise = Vco921Calibration::triRiseShare;
        const double x = 2.0 * (ph < rise ? ph / rise : (1.0 - ph) / (1.0 - rise)) - 1.0;
        // Both ramps sample x uniformly, so centering x² at 1/3 preserves the zero mean.
        return float(x + Vco921Calibration::triQuadratic * (x * x - 1.0 / 3.0));
    }

    static float sine(double ph) {
        // Keep the sine's underlying phase independent of the triangle's slope asymmetry.
        const float x = std::sin(kHalfPi * float(ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph));
        constexpr auto& harmonics = Vco921Calibration::sineHarmonics;
        // At x = ±1, even terms cancel out of the swing and odd terms add to it.
        constexpr float halfSwing = 1.f + harmonics[1] + harmonics[3] + harmonics[5];
        float previous = 1.f;
        float current = x;
        float shaped = x;
        for (float amplitude : harmonics) {
            const float next = 2.f * x * current - previous;
            shaped += amplitude * next;
            previous = current;
            current = next;
        }
        return shaped / halfSwing;
    }

private:
    void jumpTo(double ph, float t) {
        sawBlep_.addStep(saw(ph) - saw(phase_), t);
        rectBlep_.addStep(rect(ph) - rect(phase_), t);
        triBlep_.addStep(tri(ph) - tri(phase_), t);
        sineBlep_.addStep(sine(ph) - sine(phase_), t);
        phase_ = ph;
    }

    double phase_ = 0.0;
    double width_ = 0.5;
    bool running_ = true;
    bool clampActive_ = false;
    TriggerInput vtrig_{TriggerInput::Kind::V};
    TriggerInput strig_{TriggerInput::Kind::S};
    BlepChannel sawBlep_, rectBlep_, triBlep_, sineBlep_;
};

}  // namespace k280
