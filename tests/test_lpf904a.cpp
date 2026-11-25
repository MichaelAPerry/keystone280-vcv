// Golden tests for the 904A ladder. Targets follow sim/docs/m3-specs.md.
#include <algorithm>
#include <cmath>
#include <vector>

#include "modules/Lpf904A.hpp"
#include "test_main.hpp"

using k280::Lpf904A;

static constexpr float kRate = 48000.f;
static constexpr double kTwoPi = 6.28318530717958647692;

static double db(double ratio) { return 20.0 * std::log10(ratio); }

static float cvFor(float hz) { return std::log2(hz / 1421.f); }

static double peak(const std::vector<float>& signal) {
    double value = 0.0;
    for (const float x : signal)
        value = std::max(value, double(std::abs(x)));
    return value;
}

static double rms(const std::vector<float>& signal) {
    double sum = 0.0;
    for (const float x : signal)
        sum += double(x) * x;
    return std::sqrt(sum / signal.size());
}

static double tonePeak(const std::vector<float>& signal, double hz, float rate) {
    double real = 0.0, imaginary = 0.0;
    for (std::size_t i = 0; i < signal.size(); ++i) {
        const double angle = kTwoPi * hz * i / rate;
        real += signal[i] * std::cos(angle);
        imaginary += signal[i] * std::sin(angle);
    }
    return 2.0 * std::hypot(real, imaginary) / signal.size();
}

static double measureHz(const std::vector<float>& signal, float rate) {
    double first = 0.0, last = 0.0;
    int crossings = 0;
    for (std::size_t i = 1; i < signal.size(); ++i) {
        const double prev = signal[i - 1], next = signal[i];
        if (prev < 0.0 && next >= 0.0) {
            last = i - 1 + prev / (prev - next);
            if (crossings++ == 0)
                first = last;
        }
    }
    return crossings > 1 ? (crossings - 1) * rate / (last - first) : 0.0;
}

template <typename Input>
static std::vector<float> render(float rate, const Lpf904A::Params& p, int settle,
                                 int samples, Input input) {
    Lpf904A filter;
    std::vector<float> result;
    result.reserve(samples);
    bool finite = true;
    for (int i = 0; i < settle + samples; ++i) {
        const float out = filter.process(rate, p, input(i)).out;
        finite &= std::isfinite(out);
        if (i >= settle)
            result.push_back(out);
    }
    CHECK_NEAR(finite, true, 0);
    return result;
}

static std::vector<float> tone(float cutoff, float hz, float level, float regen) {
    Lpf904A::Params p;
    p.regen = regen;
    return render(kRate, p, int(kRate / 2), int(kRate / 2), [=](int i) {
        return Lpf904A::Inputs{level * float(std::sin(kTwoPi * hz * i / kRate)), {cvFor(cutoff)}};
    });
}

static void testCutoffAndReset() {
    Lpf904A filter;
    Lpf904A::Params p;
    const float centres[] = {355.25f, 1421.f, 5684.f};
    for (int range = 0; range < 3; ++range) {
        p.range = range;
        CHECK_NEAR(filter.cutoffHz(p, 0.f), centres[range], 0.001);
        CHECK_NEAR(filter.cutoffHz(p, 1.f), 2.f * centres[range], 0.001);
        CHECK_NEAR(filter.cutoffHz(p, -1.f), 0.5f * centres[range], 0.001);
    }

    p.range = 1;
    for (const auto& point : {std::array<float, 2>{0.f, 63.f}, {0.2f, 146.5f},
                              {0.3f, 331.f}, {0.4f, 801.f}, {0.5f, 1421.f},
                              {0.6f, 2416.f}, {0.7f, 3797.f}, {0.8f, 7205.f}}) {
        p.fixedCv = point[0];
        CHECK_NEAR(filter.cutoffHz(p, 0.f), point[1], 0.002);
    }
    p.fixedCv = 0.65f;
    const double interpolatedHz = std::sqrt(2416.0 * 3797.0);
    CHECK_NEAR(filter.cutoffHz(p, 0.f), interpolatedHz, 0.002);
    CHECK_NEAR(filter.cutoffHz(p, 1.f), 2 * interpolatedHz, 0.004);
    p.range = 0;
    CHECK_NEAR(filter.cutoffHz(p, 0.f), interpolatedHz / 4, 0.001);

    p.range = 1;
    p.fixedCv = -1.f;
    CHECK_NEAR(filter.cutoffHz(p, 0.f), 63, 0.001);
    CHECK_NEAR(filter.cutoffHz(p, -10.f), 1421.0 / 128, 0.001);
    CHECK_NEAR(filter.cutoffHz(p, 5.f), 63 * 32, 0.001);
    p.fixedCv = 1.f;
    const double extrapolatedHz = 7205.0 * std::pow(7205.0 / 3797.0, 2);
    CHECK_NEAR(filter.cutoffHz(p, 0.f), extrapolatedHz, 0.02);
    CHECK_NEAR(filter.cutoffHz(p, 10.f), 1421.0 * 128, 0.01);
    p.fixedCv = 2.f;
    CHECK_NEAR(filter.cutoffHz(p, 0.f), extrapolatedHz, 0.02);
    p.range = -1;
    CHECK_NEAR(filter.cutoffHz(p, 0.f), extrapolatedHz / 4, 0.01);
    p.range = 3;
    CHECK_NEAR(filter.cutoffHz(p, 0.f), extrapolatedHz * 4, 0.08);

    p = {};
    Lpf904A reference;
    double difference = 0.0;
    for (int i = 0; i < 1000; ++i) {
        const float sample = i < 60 ? 1.f : -1.f;
        const float split = filter.process(kRate, p, {sample, {0.5f, -0.25f, 0.75f}}).out;
        const float summed = reference.process(kRate, p, {sample, {1.f}}).out;
        difference = std::max(difference, double(std::abs(split - summed)));
    }
    CHECK_NEAR(difference, 0.0, 1e-7);

    // Compare values on either side of the host-rate guard with identical drive.
    p.range = 2;
    filter.reset();
    reference.reset();
    const float limitCv = cvFor(0.45f * kRate) - 2.f;
    difference = 0.0;
    for (int i = 0; i < 100; ++i) {
        const float sample = i == 0 ? 1.f : 0.f;
        const float capped = filter.process(kRate, p, {sample, {7.f}}).out;
        const float atLimit = reference.process(kRate, p, {sample, {limitCv}}).out;
        difference = std::max(difference, double(std::abs(capped - atLimit)));
    }
    CHECK_NEAR(difference, 0.0, 1e-6);

    Lpf904A fresh;
    filter.cal.inputDriveV = fresh.cal.inputDriveV = 3.f;
    filter.reset();
    p.regen = 1.f;
    difference = 0.0;
    for (int i = 0; i < 1000; ++i) {
        const Lpf904A::Inputs in{2.f, {1.f}};
        difference = std::max(difference, double(std::abs(
            filter.process(kRate, p, in).out - fresh.process(kRate, p, in).out)));
    }
    CHECK_NEAR(difference, 0.0, 0.0);
}

static void testResponse() {
    constexpr float cutoff = 400.f;
    constexpr float amplitude = 0.025f;
    // At REGEN 0 the poles sit at passbandPoleRatio x the knob-table pitch
    // (bench 2026-11-23, filtersweep_163544); the regen-0 checks place the poles at `cutoff`.
    const float ratio0 = k280::Lpf904ACalibration{}.passbandPoleRatio;
    const auto gain = [=](float hz, float regen) {
        const float knobHz = regen == 0.f ? cutoff / ratio0 : cutoff;
        return tonePeak(tone(knobHz, hz, amplitude, regen), hz, kRate) / amplitude;
    };
    CHECK_NEAR(db(gain(40.f, 0.f)), 0.0, 0.25);
    // Middle range, FIXED noon, 0 V, REGEN 0: combined -3 dB corner 352.5 Hz.
    CHECK_NEAR(db(tonePeak(tone(1421.f, 352.5f, amplitude, 0.f), 352.5f, kRate) / amplitude),
               -3.01, 0.3);
    CHECK_NEAR(db(gain(1.f, 0.f) / gain(40.f, 0.f)), -7.0, 0.5);
    // Four equal poles each contribute -3.01 dB at the pole frequency.
    CHECK_NEAR(db(gain(cutoff, 0.f)), -12.04, 1.0);
    const double slope = db(gain(4 * cutoff, 0.f) / gain(8 * cutoff, 0.f));
    std::printf("  4fc to 8fc attenuation %.2f dB/oct\n", slope);
    CHECK_NEAR(slope, 24.0, 3.0);

    const auto dc = render(kRate, {}, int(kRate), 4096, [](int) {
        return Lpf904A::Inputs{0.5f, {}};
    });
    CHECK_NEAR(peak(dc), 0.0, 0.001);

    // Sweep the band around the peak as its frequency moves with regeneration.
    double previousPeak = 0.0;
    for (const float regen : {0.51f, 0.59f, 0.63f}) {
        double resonance = 0.0;
        for (float hz = 320.f; hz <= 560.f; hz += 20.f)
            resonance = std::max(resonance, gain(hz, regen));
        CHECK_NEAR(resonance > previousPeak, true, 0);
        previousPeak = resonance;
    }
    CHECK_NEAR(previousPeak > 1.0, true, 0);
    CHECK_NEAR(gain(40.f, 0.63f) < 0.5 * gain(40.f, 0.f), true, 0);
}

static void testOscillation() {
    const auto silence = [](int) { return Lpf904A::Inputs{}; };
    const float benchHz[] = {355.25f, 1421.f, 5684.f};
    for (int range = 0; range < 3; ++range) {
        const auto signal = render(kRate, {.5f, range, 1.f}, int(3 * kRate), int(kRate), silence);
        CHECK_NEAR(measureHz(signal, kRate), benchHz[range], .05 * benchHz[range]);
        CHECK_NEAR(peak(signal), 1.2, .12);
    }
    const auto belowOnset = render(kRate, {.5f, 1, .65f}, int(3 * kRate), int(kRate), silence);
    CHECK_NEAR(peak(belowOnset), 0, .001);
    for (const auto& point : {std::array<float, 2>{.675f, -26.3f}, {.7f, -21.4f}}) {
        const auto signal = render(kRate, {.5f, 1, point[0]}, int(3 * kRate), int(kRate), silence);
        CHECK_NEAR(db(rms(signal) / 3.37), point[1], 1.0);
    }

    for (const float rate : {44100.f, 48000.f}) {
        Lpf904A driven;
        Lpf904A::Params p;
        p.regen = 1.f;
        Lpf904A::Inputs in{0.f, {cvFor(220.f)}};
        float loudest = 0.f;
        for (int i = 0; i < int(rate); ++i)
            loudest = std::max(loudest, std::abs(driven.process(rate, p, in).out));
        CHECK_NEAR(loudest > 1.f, true, 0);
        p.regen = 0.65f;
        for (int i = 0; i < int(rate); ++i)
            driven.process(rate, p, in);
        float residual = 0.f;
        for (int i = 0; i < int(rate / 2); ++i) {
            const float out = driven.process(rate, p, in).out;
            residual = std::isfinite(out) ? std::max(residual, std::abs(out)) : 10.f;
        }
        CHECK_NEAR(residual, 0.0, 0.001);

        for (const float regen : {0.9f, 1.f}) {
            p.regen = regen;
            for (int octave = 0; octave < 6; ++octave) {
                const float hz = std::ldexp(55.f, octave);
                in.cv[0] = cvFor(55.f) + float(octave);
                const auto signal = render(rate, p, int(3 * rate), int(0.75f * rate),
                                           [&](int) { return in; });
                const float cutoff = Lpf904A{}.cutoffHz(p, in.cv[0]);
                CHECK_NEAR(cutoff, hz, 0.001);
                const double cents = 1200.0 * std::log2(measureHz(signal, rate) / cutoff);
                const double amplitude = peak(signal);
                std::printf("  %.0f Hz rate, regen %.1f, %4.0f Hz cutoff: %+6.2f cents, %.3f V peak\n",
                            rate, regen, hz, cents, amplitude);
                CHECK_NEAR(cents, 0.0, 10.0);
                CHECK_NEAR(db(amplitude / 1.2), 0.0, 3.0);
                CHECK_NEAR(amplitude <= 3.46, true, 0);
            }
        }
    }
}

static void testOversampling() {
    // At 48 kHz a 9 kHz drive's fifth harmonic aliases directly to 3 kHz
    // in a base-rate ladder. At 2x it reaches the half-band stopband.
    constexpr float hz = 9000.f;
    const auto signal = tone(18000.f, hz, 10.f, 0.f);
    const double aliasDb = db(tonePeak(signal, 3000.f, kRate) / tonePeak(signal, hz, kRate));
    std::printf("  9 kHz drive, 3 kHz alias %.1f dBc\n", aliasDb);
    CHECK_NEAR(aliasDb < -44.0, true, 0);
}

static void testBounds() {
    Lpf904A::Params p;
    p.range = 2;
    p.regen = 1.f;
    for (const float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        for (const bool modulated : {false, true}) {
            const auto signal = render(rate, p, 0, int(rate), [=](int i) {
                const double t = i / double(rate);
                const float input = std::sin(kTwoPi * 1739 * t) >= 0.0 ? 10.f : -10.f;
                const float cv = modulated ? 8.f * float(std::sin(kTwoPi * 6871 * t)) : 7.f;
                return Lpf904A::Inputs{input, {cv, -0.5f, 0.5f}};
            });
            CHECK_NEAR(peak(signal) <= 10.0, true, 0);
        }
    }
}

static void testBookPatch() {
    Lpf904A::Params p;
    p.fixedCv = 0.3f;
    p.regen = 0.4f;
    std::vector<float> input;
    const int samples = int(kRate / 2);
    const auto out = render(kRate, p, samples, samples, [&](int i) {
        const double phase = 392.0 * i / kRate;
        const float saw = 0.69f * float(2.0 * (phase - std::floor(phase)) - 1.0);
        if (i >= samples)
            input.push_back(saw);
        return Lpf904A::Inputs{saw, {1.4f}};
    });
    const double inRms = rms(input), outRms = rms(out);
    std::printf("  Patch 1 saw RMS %.4f V in, %.4f V out\n", inRms, outRms);
    CHECK_NEAR(outRms > 0.01 && outRms < inRms, true, 0);
    const auto color = [](const std::vector<float>& x) {
        return tonePeak(x, 3 * 392.f, kRate) / tonePeak(x, 392.f, kRate);
    };
    // With Row B CV, the third harmonic is above the poles and is
    // attenuated relative to the fundamental (0.425 before the REGEN-0
    // passband fix of 2026-11-23).
    CHECK_NEAR(color(out) / color(input), 0.163, 0.01);
}

int main() {
    testCutoffAndReset();
    testResponse();
    testOscillation();
    testOversampling();
    testBounds();
    testBookPatch();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
