// Block-level Moog 921 circuit reduction with Behringer panel controls.
// Sources, equations, alignment and unresolved component values: sim/circuits/921/README.md.
#pragma once

#include "modules/Vco921.hpp"

namespace k280 {

class Vco921Circuit {
public:
    using Range = Vco921::Range;
    using AuxWave = Vco921::AuxWave;
    using ClampMode = Vco921::ClampMode;
    static constexpr Range R32 = Vco921::R32, R16 = Vco921::R16, R8 = Vco921::R8,
                           R4 = Vco921::R4, R2 = Vco921::R2, R1 = Vco921::R1,
                           NUM_RANGES = Vco921::NUM_RANGES;
    static constexpr AuxWave AUX_SINE = Vco921::AUX_SINE, AUX_TRI = Vco921::AUX_TRI,
                             AUX_INV_SAW = Vco921::AUX_INV_SAW, AUX_SAW = Vco921::AUX_SAW,
                             AUX_RECT = Vco921::AUX_RECT, AUX_INV_RECT = Vco921::AUX_INV_RECT,
                             NUM_AUX = Vco921::NUM_AUX;
    using Params = Vco921::Params;
    using Inputs = Vco921::Inputs;
    using Outputs = Vco921::Outputs;
    static constexpr Confidence kConfidence = Confidence::Schematic;

    struct Alignment {
        double referenceRate; // FREQ ADJ, Iref / (C * ramp swing) at 2'
        double scale;         // SCALE, multiplier on the base-emitter drive
        double hi;            // HI ADJ, incremental current feed-forward, seconds
    };

    static Alignment align() {
        // At each target f the capacitor must charge at f/(1-f*t_reset).
        // Invert I + HI*I^2 in normalized current units. Solve HI so the
        // required converter currents at -2, 0 and +2 V form a geometric series.
        const auto converterRate = [](double f, double hi) {
            const double chargeRate = f / (1.0 - f * resetSeconds);
            return 2.0 * chargeRate / (1.0 + std::sqrt(1.0 + 4.0 * hi * chargeRate));
        };
        double lo = 0.0, high = 4.0 * resetSeconds;
        for (int i = 0; i < 48; ++i) {
            const double hi = (lo + high) * 0.5;
            const double mid = converterRate(640.0, hi);
            if (converterRate(160.0, hi) * converterRate(2560.0, hi) > mid * mid)
                lo = hi;
            else
                high = hi;
        }
        const double hi = (lo + high) * 0.5;
        const double ref = converterRate(640.0, hi);
        return {ref, std::log2(ref / converterRate(160.0, hi)) / 2.0, hi};
    }

    Vco921Circuit() {
        // Original Moog p31 level targets, interpreted as dBu at alignment.
        cal = {160.f, 0.f, 1.f, 80000.f, 100.f, peakFromDbu(-4.0, std::sqrt(2.0)),
               nominalSawPeak_, nominalSawPeak_, peakFromDbu(-2.0, 1.0), 0.f};
        cal.auxMaxPeakV = 5.f * cal.sawPeakV;
    }

    Vco921Calibration cal;
    double temperatureK = 298.15;
    Alignment trims = nominalAlignment_;

    float frequencyHz(const Params& p, float cvSum) const {
        const double rate = chargingRate(p, cvSum);
        return float(std::min(rate / (1.0 + rate * resetSeconds), frequencyLimit(p)));
    }

    static float pulseWidth(const Params& p, float cvSum) {
        // IC510 compares the ramp with the width summer's threshold.
        return std::clamp(0.1f + 0.8f * p.width + 0.15f * cvSum, 0.1f, 0.9f);
    }

    Outputs process(float sampleRate, const Params& p, const Inputs& in) {
        if (!(sampleRate > 0.f) || !std::isfinite(sampleRate)) return {};
        const double dt = 1.0 / sampleRate;
        const double limit = std::min(frequencyLimit(p), 0.45 * sampleRate);
        const double rate = std::min(chargingRate(p, in.fcv[0] + in.fcv[1] + in.fcv[2]),
                                     limit / (1.0 - limit * resetSeconds));
        const float oldRect = rect(charge_);
        width_ = pulseWidth(p, in.pw[0] + in.pw[1]);
        rectBlep_.addStep(rect(charge_) - oldRect, 0.f);

        float vTime = 0.f, sTime = 0.f;
        const bool vEdge = vtrig_.process(in.vtrig, in.vtrigConnected, &vTime) && vtrig_.active();
        const bool sEdge = strig_.process(in.strig, in.strigConnected, &sTime) && strig_.active();
        const double point = 0.1 + 0.8 * std::clamp(p.clampPoint, 0.f, 1.f);
        double elapsed = 0.0;
        const auto clampAt = [&](float fraction) {
            advance(fraction * dt, elapsed, dt, rate);
            jumpTo(point, fraction);
            resetLeft_ = 0.0;
            // The schematic's one-shot is the default. Reset is the shared
            // interface's immediate-reset variant for patch compatibility.
            clampLeft_ = p.clampMode == ClampMode::Hold ? 0.001 : 0.0;
        };
        if (vEdge && (!sEdge || vTime <= sTime)) clampAt(vTime);
        if (sEdge && (!vEdge || vTime != sTime)) clampAt(sTime);
        if (vEdge && sEdge && vTime > sTime) clampAt(vTime);
        advance(dt, elapsed, dt, rate);

        Outputs o;
        o.saw = cal.sawPeakV * sawBlep_.emit(saw(charge_));
        o.rect = cal.rectPeakV * rectBlep_.emit(rect(charge_));
        o.tri = cal.triPeakV * triBlep_.emit(tri(charge_));
        o.sine = cal.sinePeakV * sineBlep_.emit(sine(charge_));
        float selected = 0.f;
        switch (p.auxWave) {
            case AUX_SINE: selected = o.sine; break;
            case AUX_TRI: selected = o.tri; break;
            case AUX_SAW: selected = o.saw; break;
            case AUX_INV_SAW: selected = -o.saw; break;
            case AUX_RECT: selected = 0.5f * (o.rect + cal.rectPeakV); break;
            case AUX_INV_RECT: selected = 0.5f * (cal.rectPeakV - o.rect); break;
            default: break;
        }
        o.aux = selected * (cal.auxMaxPeakV / nominalSawPeak_) *
                std::clamp(p.auxLevel, 0.f, 1.f);
        o.auxNeg = -o.aux;
        return o;
    }

    double phase() const { return charge_; }
    bool clamped() const { return clampLeft_ > 0.0; }
    bool running() const { return !clamped() && resetLeft_ == 0.0; }

    static float saw(double charge) { return float(1.0 - 2.0 * charge); }
    float rect(double charge) const { return charge < width_ ? 1.f : -1.f; }
    static float tri(double charge) { return float(1.0 - 2.0 * std::fabs(2.0 * charge - 1.0)); }
    static float sine(double charge) {
        // SG3821 differential pair. Shape trim sets the peak differential
        // drive to 2*Vt*1.4, with a centered bias and matched collector loads.
        return float(std::tanh(1.4 * tri(charge)) / 0.8853516482022625);
    }

private:
    static constexpr double resetSeconds = 10e-6;  // ASSUMPTION, free-running FET reset
    static constexpr double nominalTemperature = 298.15;
    static constexpr double boltzmannOverCharge = 8.617333262e-5; // V/K

    static float peakFromDbu(double db, double crestFactor) {
        return float(0.775 * crestFactor * std::pow(10.0, db / 20.0));
    }

    inline static const Alignment nominalAlignment_ = align();
    inline static const float nominalSawPeak_ = peakFromDbu(-6.0, std::sqrt(3.0));

    double frequencyLimit(const Params& p) const {
        const double ceiling = p.subAudio ? cal.audioMaxHz / cal.subAudioDivisor : cal.audioMaxHz;
        return std::clamp(ceiling, 0.0, 0.999 / resetSeconds);
    }

    double chargingRate(const Params& p, float cv) const {
        const double oct = std::clamp(p.range, 0, NUM_RANGES - 1) - R2 +
                           (p.frequency - 0.5) * (p.semitoneScale ? 2.0 : 12.0) +
                           cal.unitOffsetSemitones / 12.0;
        const double sumNode = -0.5 * (oct + cv / cal.voltsPerOctave);
        const double temperature = std::clamp(temperatureK, 200.0, 400.0);
        const double vt = boltzmannOverCharge * temperature;
        const double tempco = 1.0 + 0.0033 * (temperature - nominalTemperature);
        const double drive = -2.0 * sumNode * trims.scale * std::log(2.0) *
                             boltzmannOverCharge * nominalTemperature * tempco;
        const double current = trims.referenceRate * (cal.hzAt8ftCentre / 160.0) *
                               std::exp(std::clamp(drive / vt, -60.0, 30.0));
        const double rate = current * (1.0 + trims.hi * current);
        return p.subAudio ? rate / cal.subAudioDivisor : rate;
    }

    void jumpTo(double next, float time) {
        sawBlep_.addStep(saw(next) - saw(charge_), time);
        rectBlep_.addStep(rect(next) - rect(charge_), time);
        triBlep_.addStep(tri(next) - tri(charge_), time);
        sineBlep_.addStep(sine(next) - sine(charge_), time);
        charge_ = next;
    }

    void advance(double end, double& elapsed, double dt, double rate) {
        while (elapsed < end) {
            double& hold = clampLeft_ > 0.0 ? clampLeft_ : resetLeft_;
            if (hold > 0.0) {
                const double remaining = end - elapsed;
                if (hold > remaining) {
                    hold -= remaining;
                    elapsed = end;
                    return;
                }
                elapsed += hold;
                hold = 0.0;
                if (charge_ == 1.0) jumpTo(0.0, float(elapsed / dt));
                continue;
            }
            const double next = charge_ + rate * (end - elapsed);
            const double stop = std::min(next, 1.0);
            if (charge_ < width_ && stop >= width_)
                rectBlep_.addStep(-2.f, float((elapsed + (width_ - charge_) / rate) / dt));
            if (next < 1.0) {
                charge_ = next;
                elapsed = end;
            } else {
                elapsed += (1.0 - charge_) / rate;
                charge_ = 1.0;
                resetLeft_ = resetSeconds;
            }
        }
    }

    double charge_ = 0.0, width_ = 0.5;
    double resetLeft_ = 0.0, clampLeft_ = 0.0;
    TriggerInput vtrig_{TriggerInput::Kind::V}, strig_{TriggerInput::Kind::S};
    BlepChannel sawBlep_, rectBlep_, triBlep_, sineBlep_;
};

}  // namespace k280
