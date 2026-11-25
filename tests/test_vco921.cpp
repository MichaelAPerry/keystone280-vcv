// Golden tests for the 921 VCO model (sim/engine/modules/Vco921.hpp).
// Expectations come from the bench, the Behringer guide, and the model assumptions.
#include <vector>

#include "Knob.hpp"
#include "models/Blep.hpp"
#include "modules/Vco921.hpp"
#include "test_main.hpp"

using k280::Vco921;

static double cents(double a, double b) { return 1200.0 * std::log2(a / b); }

// Run the VCO and collect one output.
struct Run {
    std::vector<float> sine, tri, saw, rect, aux, auxNeg;
};

template <typename InputFn>
static Run render(Vco921::Params p, float sr, int n, InputFn inputs, Vco921 v = Vco921()) {
    Run r;
    for (int i = 0; i < n; ++i) {
        Vco921::Inputs in;
        inputs(i, in);
        auto o = v.process(sr, p, in);
        r.sine.push_back(o.sine);
        r.tri.push_back(o.tri);
        r.saw.push_back(o.saw);
        r.rect.push_back(o.rect);
        r.aux.push_back(o.aux);
        r.auxNeg.push_back(o.auxNeg);
        if (!(std::isfinite(o.sine) && std::isfinite(o.tri) && std::isfinite(o.saw) && std::isfinite(o.rect) &&
              std::isfinite(o.aux))) {
            CHECK_NEAR(0.0, 1.0, 0.0);  // non-finite sample
            break;
        }
    }
    return r;
}

static Run render(Vco921::Params p, float sr, int n, float cv = 0.f) {
    return render(p, sr, n, [cv](int, Vco921::Inputs& in) { in.fcv[0] = cv; });
}

// Frequency from the first and last rising zero crossings of the sine (interpolated).
static double measureHz(const std::vector<float>& x, float sr) {
    double first = -1, last = -1;
    int count = 0;
    for (size_t i = 1; i < x.size(); ++i) {
        if (x[i - 1] < 0.f && x[i] >= 0.f) {
            double t = (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    }
    return count > 0 ? count * sr / (last - first) : 0.0;
}

static float peak(const std::vector<float>& x, size_t from = 100) {
    float m = 0.f;
    for (size_t i = from; i < x.size(); ++i) m = std::max(m, std::fabs(x[i]));
    return m;
}

static double rms(const std::vector<float>& x, size_t from = 100) {
    double s = 0;
    for (size_t i = from; i < x.size(); ++i) s += double(x[i]) * x[i];
    return std::sqrt(s / (x.size() - from));
}

static double duty(const std::vector<float>& x, size_t from = 100) {
    size_t hi = 0;
    for (size_t i = from; i < x.size(); ++i) hi += x[i] > 0.f;
    return double(hi) / (x.size() - from);
}

static void testKnobClock() {
    CHECK_NEAR(k280::knobFromClock(7), 0.0, 1e-6);
    CHECK_NEAR(k280::knobFromClock(12), 0.5, 1e-6);
    CHECK_NEAR(k280::knobFromClock(2), 0.7, 1e-6);   // 2 o'clock
    CHECK_NEAR(k280::knobFromClock(5), 1.0, 1e-6);   // 5 o'clock
    CHECK_NEAR(k280::clockFromKnob(0.75f), 14.5, 1e-5);
}

// 1 V/oct tracking over six octaves (C1..C7 at 8'), measured from the rendered sine.
static void testTracking() {
    const float sr = 96000.f;
    Vco921::Params p;
    for (int v = -3; v <= 3; ++v) {
        double expect = 261.6256 * std::pow(2.0, v);
        Run r = render(p, sr, int(sr * 2), float(v));
        double got = measureHz(r.sine, sr);
        CHECK_NEAR(cents(got, expect), 0.0, 1.0);
    }
    // Fractional volts: a fifth (7/12 V).
    Run r = render(p, sr, int(sr), 7.f / 12.f);
    CHECK_NEAR(cents(measureHz(r.sine, sr), 261.6256 * std::pow(2.0, 7.0 / 12)), 0.0, 1.0);
    // The three FCV inputs sum.
    Run s = render(p, sr, int(sr), [](int, Vco921::Inputs& in) {
        in.fcv[0] = 0.5f; in.fcv[1] = 0.25f; in.fcv[2] = 0.25f; });
    CHECK_NEAR(cents(measureHz(s.sine, sr), 523.2511), 0.0, 1.0);
}

static void testRangeAndKnob() {
    Vco921 v;
    Vco921::Params p;
    const float hz8 = 261.6256f;
    const float ranges[] = {0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f};  // 32' .. 1'
    for (int r = 0; r < Vco921::NUM_RANGES; ++r) {
        p.range = r;
        CHECK_NEAR(v.frequencyHz(p, 0.f), hz8 * ranges[r], 0.01);
    }
    p.range = Vco921::R8;
    p.frequency = 0.f;  // fully counter-clockwise: -6 octaves
    CHECK_NEAR(v.frequencyHz(p, 0.f), hz8 / 64, 0.001);
    p.frequency = 1.f;  // +6 octaves
    CHECK_NEAR(v.frequencyHz(p, 0.f), hz8 * 64, 0.5);
    p.semitoneScale = true;  // ±12 semitones
    CHECK_NEAR(v.frequencyHz(p, 0.f), hz8 * 2, 0.01);
    p.frequency = 0.f;
    CHECK_NEAR(v.frequencyHz(p, 0.f), hz8 / 2, 0.01);
    // Guide: audio range typically 1 Hz .. 80 kHz; sub-audio 0.01 Hz .. 800 Hz.
    p.semitoneScale = false;
    p.range = Vco921::R32;
    CHECK_NEAR(v.frequencyHz(p, 0.f), 1.02, 0.01);
    p.subAudio = true;
    CHECK_NEAR(v.frequencyHz(p, 0.f), 0.0102, 0.0001);
    p.range = Vco921::R1;
    p.frequency = 1.f;
    CHECK_NEAR(v.frequencyHz(p, 0.f), 800.0, 1e-3);   // capped
    p.subAudio = false;
    CHECK_NEAR(v.frequencyHz(p, 0.f), 80000.0, 1e-1);  // capped
    // Sub-audio renders: 8' noon / 100 = 2.616 Hz.
    Vco921::Params q;
    q.subAudio = true;
    Run r = render(q, 48000.f, 48000 * 10);
    CHECK_NEAR(measureHz(r.sine, 48000.f), 2.616256, 0.01);
}

static void testUnitOffset() {
    Vco921 a, b;
    b.cal.unitOffsetSemitones = k280::Vco921Calibration::unitBOffsetSemitones;
    Vco921::Params p;
    for (int range = 0; range < Vco921::NUM_RANGES; ++range) {
        p.range = range;
        const float hz = b.frequencyHz(p, 0.f);
        CHECK_NEAR(cents(hz, a.frequencyHz(p, 0.f)), -185.0, 0.001);
        CHECK_NEAR(b.frequencyHz(p, 1.f) / hz, 2.0, 1e-6);
    }
}

static void testWaveforms() {
    const float sr = 48000.f;
    Vco921::Params p;
    p.range = Vco921::R32;  // 65 Hz: plenty of samples per cycle
    Run r = render(p, sr, int(sr));
    const auto swing = [](const std::vector<float>& wave) {
        const auto [lo, hi] = std::minmax_element(wave.begin() + 100, wave.end());
        return *hi - *lo;
    };
    CHECK_NEAR(swing(r.sine), 1.490, 0.005);
    CHECK_NEAR(swing(r.tri), 1.622, 0.005);
    CHECK_NEAR(swing(r.saw), 2.2, 0.01);
    CHECK_NEAR(swing(r.rect), 1.1, 0.005);
    CHECK_NEAR(rms(r.sine), 0.523, 0.003);
    CHECK_NEAR(duty(r.rect), 0.5, 0.01);
    // Most samples slope down. Each reset crosses zero in the positive direction.
    int falling = 0, resets = 0;
    for (size_t i = 101; i < r.saw.size(); ++i) {
        falling += r.saw[i] < r.saw[i - 1];
        resets += r.saw[i - 1] < 0.f && r.saw[i] >= 0.f;
    }
    CHECK_NEAR(double(falling) / (r.saw.size() - 101), 1.0, 0.01);
    CHECK_NEAR(resets, Vco921().frequencyHz(p, 0.f), 1.0);
}

static void testBenchShape() {
    // Oversample so the sampled extrema give a precise mid-swing threshold.
    constexpr float sr = 192000.f;
    constexpr int period = 856;
    Vco921 v;
    v.cal.hzAt8ftCentre = sr / period;  // ~224 Hz, coherent over a complete cycle
    Vco921::Params p;
    const Run r = render(p, sr, 3 * period, [](int, Vco921::Inputs&) {}, v);
    const auto magnitude = [](const std::vector<float>& wave, int harmonic) {
        double re = 0.0, im = 0.0;
        for (int i = 0; i < period; ++i) {
            const double angle = 2.0 * M_PI * harmonic * i / period;
            re += wave[period + i] * std::cos(angle);
            im += wave[period + i] * std::sin(angle);
        }
        return std::hypot(re, im);
    };
    // Test 20, dB relative to H1. The triangle's existing odd harmonics stay within 1 dB.
    const double sineTargets[] = {-35.4, -34.7, -54.6, -43.3, -61.0, -48.7, -64.0};
    const double triTargets[] = {-33.8, -19.4, -41.1, -28.5, -45.5, -34.6, -48.6};
    const double sineH1 = magnitude(r.sine, 1);
    const double triH1 = magnitude(r.tri, 1);
    for (int h = 2; h <= 8; ++h) {
        const double tolerance = h <= 5 ? 3.0 : 5.0;
        CHECK_NEAR(20.0 * std::log10(magnitude(r.sine, h) / sineH1), sineTargets[h - 2], tolerance);
        CHECK_NEAR(20.0 * std::log10(magnitude(r.tri, h) / triH1), triTargets[h - 2],
                   h % 2 ? 1.0 : tolerance);
    }

    const auto begin = r.tri.begin() + period;
    const auto [lo, hi] = std::minmax_element(begin, begin + period);
    CHECK_NEAR(double(hi - begin) / period, 0.5114, 1.0 / period);
    const double mid = (*lo + *hi) / 2.0;
    double risingAt = 0.0, fallingAt = 0.0;
    for (int i = period; i < 2 * period; ++i) {
        const double a = r.tri[i], b = r.tri[i + 1];
        if (a < mid && b >= mid) risingAt = i + (mid - a) / (b - a);
        if (a > mid && b <= mid) fallingAt = i + (mid - a) / (b - a);
    }
    CHECK_NEAR(1.0 - (fallingAt - risingAt) / period, 0.5114, 0.001);
}

static void testPulseWidth() {
    const float sr = 48000.f;
    Vco921::Params p;
    p.range = Vco921::R32;
    p.width = k280::knobFromClock(3);  // 3 o'clock = knob 0.8 -> 74 %
    CHECK_NEAR(duty(render(p, sr, int(sr)).rect), 0.74, 0.01);
    p.width = 0.5f;
    Run r = render(p, sr, int(sr), [](int, Vco921::Inputs& in) { in.pw[0] = 1.f; });  // +15 %
    CHECK_NEAR(duty(r.rect), 0.65, 0.01);
    r = render(p, sr, int(sr), [](int, Vco921::Inputs& in) { in.pw[0] = -1.f; in.pw[1] = -1.f; });
    CHECK_NEAR(duty(r.rect), 0.20, 0.01);
    r = render(p, sr, int(sr), [](int, Vco921::Inputs& in) { in.pw[0] = 6.f; });  // limit 90 %
    CHECK_NEAR(duty(r.rect), 0.90, 0.01);
    p.width = 0.f;
    CHECK_NEAR(duty(render(p, sr, int(sr)).rect), 0.10, 0.01);  // knob limit 10 %
}

static void testAux() {
    const float sr = 48000.f;
    Vco921::Params p;
    p.range = Vco921::R32;
    p.auxLevel = 1.f;
    p.auxWave = Vco921::AUX_SINE;
    Run r = render(p, sr, int(sr));
    // The aux gain still gives the calibrated full-level swing with the shaped sine.
    const auto [lo, hi] = std::minmax_element(r.aux.begin() + 100, r.aux.end());
    CHECK_NEAR(*hi - *lo, 6.930, 0.005);
    double maxDiff = 0;
    double worst = 0;
    const k280::Vco921Calibration cal;
    for (size_t i = 0; i < r.aux.size(); ++i) {
        maxDiff = std::max(maxDiff, double(std::fabs(r.aux[i] + r.auxNeg[i])));
        worst = std::max(worst, std::fabs(r.aux[i] - double(cal.auxMaxPeakV) / cal.sinePeakV * r.sine[i]));
    }
    CHECK_NEAR(maxDiff, 0.0, 1e-6);
    CHECK_NEAR(worst, 0.0, 1e-4);
    p.auxWave = Vco921::AUX_TRI;
    r = render(p, sr, 4000);
    double k = double(cal.auxMaxPeakV) / cal.triPeakV;
    worst = 0;
    for (size_t i = 0; i < r.aux.size(); ++i) worst = std::max(worst, std::fabs(r.aux[i] - k * r.tri[i]));
    CHECK_NEAR(worst, 0.0, 1e-4);
    p.auxWave = Vco921::AUX_INV_SAW;
    r = render(p, sr, 4000);
    k = 3.4650 / 1.10;
    worst = 0;
    for (size_t i = 0; i < r.aux.size(); ++i) worst = std::max(worst, std::fabs(r.aux[i] + k * r.saw[i]));
    CHECK_NEAR(worst, 0.0, 1e-4);
    p.auxWave = Vco921::AUX_INV_RECT;
    p.auxLevel = 0.5f;
    r = render(p, sr, 4000);
    k = 3.4650 / 0.55;
    worst = 0;
    for (size_t i = 0; i < r.aux.size(); ++i) worst = std::max(worst, std::fabs(r.aux[i] + 0.5 * k * r.rect[i]));
    CHECK_NEAR(worst, 0.0, 1e-4);
    p.auxLevel = 0.f;
    CHECK_NEAR(peak(render(p, sr, 4000).aux), 0.0, 1e-9);
}

// Largest |x[n] - x[n - period]| after settling: 0 when the output repeats every `period` samples.
static double periodicError(const std::vector<float>& x, int period, size_t from) {
    double e = 0;
    for (size_t i = from; i < x.size(); ++i) e = std::max(e, double(std::fabs(x[i] - x[i - period])));
    return e;
}

// Patch 23: VCO 1's rectangle into VCO 2's clamp input locks VCO 2 to VCO 1's period.
static void testClampSync() {
    const float sr = 48000.f;
    const int period = 200;  // master at 240 Hz, exactly 200 samples
    auto master = [](int i) { return (i % period) < period / 2 ? 5.f : 0.f; };
    Vco921::Params p;
    p.frequency = 0.5f + std::log2(360.f / 261.6256f) / 12.f;  // slave free-runs at 360 Hz
    for (auto mode : {Vco921::ClampMode::Hold, Vco921::ClampMode::Reset}) {
        p.clampMode = mode;
        Run free = render(p, sr, 9600);
        Run synced = render(p, sr, 9600, [&](int i, Vco921::Inputs& in) {
            in.vtrig = master(i); in.vtrigConnected = true; });
        CHECK_NEAR(periodicError(free.saw, period, 4000) > 0.3, 1.0, 0.0);  // not locked
        CHECK_NEAR(periodicError(synced.saw, period, 4000), 0.0, 1e-3);
        CHECK_NEAR(periodicError(synced.sine, period, 4000), 0.0, 1e-3);
    }
    // Hold: while the trigger is high the core sits at the clamping point.
    p.clampMode = Vco921::ClampMode::Hold;
    p.clampPoint = 0.5f;  // 50 % of the cycle: saw at 0 V
    Run h = render(p, sr, 9600, [&](int i, Vco921::Inputs& in) { in.vtrig = master(i); in.vtrigConnected = true; });
    CHECK_NEAR(h.saw[4050], 0.0, 1e-4);   // mid-hold (sample 4000 starts a high half; +1 latency)
    CHECK_NEAR(h.tri[4050], Vco921().cal.triPeakV * Vco921::tri(0.5), 1e-4);
    p.clampPoint = 1.f;  // 90 %
    h = render(p, sr, 9600, [&](int i, Vco921::Inputs& in) { in.vtrig = master(i); in.vtrigConnected = true; });
    CHECK_NEAR(h.saw[4050], -1.10 * 0.8, 1e-4);
    // S-trigger is active-low and needs a cable: 0 V pulses clamp; an unpatched jack never does.
    Run s = render(p, sr, 9600, [&](int i, Vco921::Inputs& in) {
        in.strig = master(i) > 0.f ? 5.f : 0.f; in.strigConnected = true; });
    CHECK_NEAR(periodicError(s.saw, period, 4000), 0.0, 1e-3);
    CHECK_NEAR(s.saw[4150], -1.10 * 0.8, 1e-4);  // held during the 0 V half
    Run u = render(p, sr, 9600, [&](int, Vco921::Inputs& in) { in.strig = 0.f; in.strigConnected = false; });
    CHECK_NEAR(periodicError(u.saw, period, 4000) > 0.3, 1.0, 0.0);
    // A V-trig below threshold (2 V) does nothing.
    Run low = render(p, sr, 9600, [&](int i, Vco921::Inputs& in) {
        in.vtrig = master(i) > 0.f ? 2.f : 0.f; in.vtrigConnected = true; });
    CHECK_NEAR(periodicError(low.saw, period, 4000) > 0.3, 1.0, 0.0);
}

// Band-limiting: at 4.2 kHz (48 kHz rate) the saw's aliases should be well below a naive saw's.
static void testAliasing() {
    const float sr = 48000.f;
    Vco921::Params p;
    p.range = Vco921::R1;
    p.frequency = 0.5f;  // 2093 Hz
    Run r = render(p, sr, 48000, 1.f);  // 4186 Hz
    // Energy at a frequency that is neither a harmonic nor near one (DFT at 1000 Hz).
    auto bin = [&](const std::vector<float>& x, double f) {
        double re = 0, im = 0;
        for (size_t i = 0; i < x.size(); ++i) {
            double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (x.size() - 1));
            re += w * x[i] * std::cos(2 * M_PI * f * i / sr);
            im += w * x[i] * std::sin(2 * M_PI * f * i / sr);
        }
        return std::sqrt(re * re + im * im);
    };
    std::vector<float> naive(r.saw.size());
    double ph = 0;
    for (size_t i = 0; i < naive.size(); ++i) {
        naive[i] = float(1.10 * (1 - 2 * ph));
        ph += 4186.01 / sr;
        ph -= std::floor(ph);
    }
    // Find the strongest alias of the naive saw below the fundamental, then compare there.
    double bestF = 0, bestNaive = 0;
    for (double f = 100; f < 4000; f += 10) {
        double m = bin(naive, f);
        if (m > bestNaive) { bestNaive = m; bestF = f; }
    }
    double reduction = 20 * std::log10(bin(r.saw, bestF) / bestNaive);
    std::printf("  strongest naive alias %.0f Hz; polyBLEP reduces it by %.1f dB\n", bestF, -reduction);
    CHECK_NEAR(reduction < -20.0, 1.0, 0.0);
}

// polyBLEP residuals, exact values for a step of +2 at three times within the sample.
static void testBlep() {
    struct Case { float frac, prev, cur; } cases[] = {{0.f, 0.f, 1.f}, {0.5f, -0.75f, 0.75f}, {1.f, -1.f, 0.f}};
    for (auto c : cases) {
        k280::BlepChannel b;
        b.emit(-1.f);              // sample n-1 (naive -1)
        b.addStep(2.f, c.frac);    // step from -1 to +1 during sample n
        float prev = b.emit(1.f);  // returns corrected n-1
        float cur = b.emit(1.f);   // returns corrected n
        CHECK_NEAR(prev, c.prev, 1e-6);
        CHECK_NEAR(cur, c.cur, 1e-6);
    }
}

// Edges and wraps landing exactly on sample points (12 kHz at 48 kHz) are band-limited too:
// the rectangle reads +a, 0, -a, 0 ... and never jumps straight between +a and -a.
static void testExactBoundaries() {
    Vco921 v;
    v.cal.hzAt8ftCentre = 12000.f;
    Vco921::Params p;
    Run r = render(p, 48000.f, 400, [](int, Vco921::Inputs&) {}, v);
    const float a = v.cal.rectPeakV;
    int raw = 0, zeros = 0;
    for (size_t i = 101; i < r.rect.size(); ++i) {
        raw += std::fabs(r.rect[i] - r.rect[i - 1]) > 1.5f * a;
        zeros += std::fabs(r.rect[i]) < 1e-6f;
    }
    CHECK_NEAR(raw, 0, 0);
    CHECK_NEAR(double(zeros) / (r.rect.size() - 101), 0.5, 0.01);
    const float sawPeak = v.cal.sawPeakV;
    int sawRaw = 0;
    for (size_t i = 101; i < r.saw.size(); ++i) sawRaw += r.saw[i] - r.saw[i - 1] > 1.5f * sawPeak;
    CHECK_NEAR(sawRaw, 0, 0);
}

// Narrowing the width while the core is held flips the rectangle; that step is band-limited.
static void testWidthChangeWhileHeld() {
    Vco921::Params p;
    p.clampPoint = (0.25f - 0.1f) / 0.8f;  // hold at 25 % of the cycle
    Run r = render(p, 48000.f, 400, [](int i, Vco921::Inputs& in) {
        in.vtrig = 5.f; in.vtrigConnected = true;
        in.pw[0] = i < 200 ? 0.f : -3.f;  // width 50 % -> 10 % at sample 200
    });
    const float a = 0.55f;
    CHECK_NEAR(r.rect[150], a, 1e-5);
    CHECK_NEAR(r.rect[300], -a, 1e-5);
    int between = 0;
    for (int i = 190; i < 210; ++i) between += std::fabs(r.rect[i]) < 0.99f * a;
    CHECK_NEAR(between > 0, 1.0, 0.0);
}

// Reset keeps running after the jump; Hold stays put. Both clamp at the same instant.
static void testResetDiffersFromHold() {
    const int period = 200;
    Vco921::Params p;
    p.frequency = 0.5f + std::log2(360.f / 261.6256f) / 12.f;
    p.clampPoint = 0.5f;
    p.clampMode = Vco921::ClampMode::Reset;
    Run r = render(p, 48000.f, 9600, [&](int i, Vco921::Inputs& in) {
        in.vtrig = (i % period) < period / 2 ? 5.f : 0.f; in.vtrigConnected = true; });
    // Edge crosses 2.5 V halfway through sample 4000; output lags one sample.
    double phase = 0.5 + (360.0 / 48000.0) * 49.5;
    CHECK_NEAR(r.saw[4050], 1.10 * (1 - 2 * phase), 2e-3);
}

// Two clamp inputs: the combined clamp follows the OR of both, at the right sub-sample times.
static void testTwoTriggerHandoff() {
    const float sr = 48000.f;
    Vco921::Params p;
    p.clampMode = Vco921::ClampMode::Hold;
    p.clampPoint = 0.5f;
    Vco921 v;
    Vco921::Inputs in;
    in.vtrigConnected = in.strigConnected = true;
    in.vtrig = 5.f; in.strig = 0.f;  // both active
    v.process(sr, p, in);
    v.process(sr, p, in);
    double inc = v.frequencyHz(p, 0.f) / sr;
    in.vtrig = 0.f;  // V releases at 0.7 of the sample ((1.5 - 5) / (0 - 5))
    in.strig = 5.f;  // S releases at 0.4 ((2 - 0) / (5 - 0))
    v.process(sr, p, in);
    CHECK_NEAR(v.phase(), 0.5 + inc * 0.3, 1e-9);  // held until the later release
    // Reset: V releases at 0.7, S asserts at 0.8: the core jumps back to the clamp point at 0.8.
    p.clampMode = Vco921::ClampMode::Reset;
    Vco921 w;
    in.vtrig = 5.f; in.strig = 5.f;
    w.process(sr, p, in);
    w.process(sr, p, in);
    in.vtrig = 0.f; in.strig = 0.f;  // S: (1 - 5) / (0 - 5) = 0.8
    w.process(sr, p, in);
    CHECK_NEAR(w.phase(), 0.5 + inc * 0.2, 1e-9);
}

// 0.01 Hz territory: the core still moves (needs double-precision phase).
static void testVerySlow() {
    Vco921::Params p;
    p.subAudio = true;
    p.range = Vco921::R32;
    p.frequency = 0.f;
    Vco921 v;
    Vco921::Inputs in;
    in.fcv[0] = -2.f;  // ~0.0026 Hz
    for (int i = 0; i < 48000; ++i) v.process(48000.f, p, in);
    double expect = v.frequencyHz(p, -2.f);  // cycles in one second
    CHECK_NEAR(v.phase(), expect, expect * 1e-3);
}

int main() {
    testKnobClock();
    testTracking();
    testRangeAndKnob();
    testUnitOffset();
    testWaveforms();
    testBenchShape();
    testPulseWidth();
    testAux();
    testClampSync();
    testAliasing();
    testBlep();
    testExactBoundaries();
    testWidthChangeWhileHeld();
    testResetDiffersFromHold();
    testTwoTriggerHandoff();
    testVerySlow();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
