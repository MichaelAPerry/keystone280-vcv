// Golden tests for the 904B. Targets follow sim/docs/m4-specs.md.
#include <algorithm>
#include <cmath>

#include "modules/Hpf904B.hpp"
#include "test_main.hpp"

using k280::Hpf904B;

static constexpr float kRate = 48000.f;
static constexpr double kTwoPi = 6.28318530717958647692;

static void testCutoff() {
    const Hpf904B filter;
    Hpf904B::Params p;
    CHECK_NEAR(p.range, 0, 0);
    const float centres[] = {282.8f, 707.1f};
    const float benchHz[] = {270.f, 740.f};  // bench 2026-11-21, tests 6c and 6d
    for (int range = 0; range < 2; ++range) {
        p = {0.5f, range};
        const float centre = centres[range];
        CHECK_NEAR(filter.cutoffHz(p, 0.f), benchHz[range], .05 * benchHz[range]);
        CHECK_NEAR(filter.cutoffHz(p, 0.f), centre, 0.001);
        CHECK_NEAR(filter.cutoffHz(p, 1.f), centre * 2.f, 0.001);
        CHECK_NEAR(filter.cutoffHz(p, -1.f), centre / 2.f, 0.001);

        p.fixedCv = 0.f;
        CHECK_NEAR(filter.cutoffHz(p, 0.f), centre / 64.f, 0.001);
        CHECK_NEAR(filter.cutoffHz(p, 5.f), centre / 2.f, 0.001);
        CHECK_NEAR(filter.cutoffHz(p, -10.f), centre / 128.f, 0.001);
        p.fixedCv = -1.f;
        CHECK_NEAR(filter.cutoffHz(p, 0.f), centre / 64.f, 0.001);

        p.fixedCv = 1.f;
        CHECK_NEAR(filter.cutoffHz(p, 0.f), centre * 64.f, 0.01);
        CHECK_NEAR(filter.cutoffHz(p, 10.f), centre * 128.f, 0.01);
        p.fixedCv = 2.f;
        CHECK_NEAR(filter.cutoffHz(p, 0.f), centre * 64.f, 0.01);
    }
    CHECK_NEAR(filter.cutoffHz({0.5f, -1}, 0.f), 282.8, 0.001);
    CHECK_NEAR(filter.cutoffHz({0.5f, 2}, 0.f), 707.1, 0.001);
}

static double toneGainDb(float cutoff, float hz) {
    Hpf904B filter;
    constexpr float amplitude = 0.025f;
    const float cv = std::log2(cutoff / 282.8f);
    const int samples = int(kRate);
    double real = 0.0, imaginary = 0.0;
    for (int i = 0; i < 2 * samples; ++i) {
        const double angle = kTwoPi * hz * i / kRate;
        const double sine = std::sin(angle);
        const float out = filter.process(kRate, {}, {amplitude * float(sine), {cv}}).out;
        if (i >= samples) {
            real += out * std::cos(angle);
            imaginary += out * sine;
        }
    }
    return 20.0 * std::log10(2.0 * std::hypot(real, imaginary) / (samples * amplitude));
}

static void testResponse() {
    for (const float cutoff : {100.f, 400.f, 2000.f, 18000.f})
        CHECK_NEAR(toneGainDb(cutoff, cutoff), -3.0, 1.0);

    constexpr float cutoff = 400.f;
    const double slope = toneGainDb(cutoff, cutoff / 4.f) - toneGainDb(cutoff, cutoff / 8.f);
    std::printf("  fc/8 to fc/4 slope %.2f dB/oct\n", slope);
    CHECK_NEAR(slope, 24.0, 3.0);
    CHECK_NEAR(toneGainDb(cutoff, 8.f * cutoff), 0.0, 0.5);
}

static void testDcAndReset() {
    Hpf904B filter;
    CHECK_NEAR(filter.process(kRate, {}, {0.5f}).out > 0.1f, true, 0);
    const int settle = int(kRate / 2);
    for (int i = 0; i < settle; ++i)
        filter.process(kRate, {}, {0.5f});

    double residual = 0.0;
    for (int i = 0; i < 4096; ++i) {
        const float out = filter.process(kRate, {}, {0.5f}).out;
        residual += double(out) * out;
    }
    CHECK_NEAR(std::sqrt(residual / 4096), 0.0, 1e-6);

    Hpf904B fresh;
    filter.cal.inputDriveV = fresh.cal.inputDriveV = 15.f;
    filter.reset();
    const float reset = filter.process(kRate, {}, {2.f}).out;
    CHECK_NEAR(reset, fresh.process(kRate, {}, {2.f}).out, 0);
}

static void testCv() {
    Hpf904B filter, reference;
    double difference = 0.0;
    for (int i = 0; i < 1000; ++i) {
        const float cv = i % 2 == 0 ? 2.f : -2.f;
        const float split = filter.process(kRate, {}, {1.f, {cv, -0.25f, 0.75f}}).out;
        const float summed = reference.process(kRate, {}, {1.f, {cv + 0.5f}}).out;
        difference += std::abs(split - summed);
    }
    CHECK_NEAR(difference, 0.0, 0.0);

    filter.reset();
    reference.reset();
    const float high = filter.process(kRate, {}, {1.f, {6.f}}).out;
    const float low = reference.process(kRate, {}, {1.f, {-6.f}}).out;
    CHECK_NEAR(low - high > 0.5f, true, 0);

    filter.reset();
    reference.reset();
    const Hpf904B::Params p{0.5f, 1};
    const float limitCv = std::log2(0.45f * kRate / 707.1f);
    const float capped = filter.process(kRate, p, {1.f, {7.f}}).out;
    CHECK_NEAR(capped, reference.process(kRate, p, {1.f, {limitCv}}).out, 1e-6);
}

static void testBounds() {
    for (const float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        for (const bool modulated : {false, true}) {
            Hpf904B filter;
            bool finite = true;
            float minimum = 0.f, maximum = 0.f;
            for (int i = 0; i < int(rate); ++i) {
                const double t = i / double(rate);
                const float input = std::sin(kTwoPi * 1739 * t) >= 0.0 ? 10.f : -10.f;
                const float cv = modulated ? 8.f * float(std::sin(kTwoPi * 6871 * t)) : -6.f;
                const float out = filter.process(rate, {0.5f, 1}, {input, {cv}}).out;
                finite &= std::isfinite(out);
                minimum = std::min(minimum, out);
                maximum = std::max(maximum, out);
            }
            CHECK_NEAR(finite, true, 0);
            CHECK_NEAR(minimum >= -10.f && maximum <= 10.f, true, 0);
            CHECK_NEAR(minimum < -8.f && maximum > 8.f, true, 0);
        }
    }
}

int main() {
    testCutoff();
    testResponse();
    testDcAndReset();
    testCv();
    testBounds();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
