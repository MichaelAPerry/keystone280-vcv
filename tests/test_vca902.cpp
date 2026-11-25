// Golden tests for the 902 VCA model, using the QSG and bench calibration.
#include "modules/Vca902.hpp"
#include "test_main.hpp"

using k280::Vca902;

static constexpr float kSampleRate = 48000.f;

static void testGainLaws() {
    Vca902 v;
    Vca902::Params p;
    CHECK_NEAR(v.gain(p, 0.15f), 0.f, 0.f);
    CHECK_NEAR(v.gain(p, 0.68f), 0.f, 0.f);  // bench 2026-11-23, linsweep_172245
    CHECK_NEAR(v.gain(p, 0.564f * 1.73f), 0.165603f, 1e-5f);
    CHECK_NEAR(v.gain(p, 0.564f * 3.06f), 0.585670f, 1e-5f);
    CHECK_NEAR(v.gain(p, 2.5f), 0.93f, 1e-6f);
    CHECK_NEAR(v.gain(p, 6.f), 0.93f, 1e-6f);

    p.exp = true;
    // Active CM1A bench points, plus a midpoint in the first dB segment.
    const float points[][2] = {
        {1.15f, -60.4f}, {1.19f, -50.7f}, {1.23f, -41.0f}, {1.31f, -30.8f},
        {1.40f, -25.4f}, {1.48f, -21.8f}, {1.56f, -19.2f}, {1.65f, -17.2f},
        {1.73f, -15.6f}, {1.81f, -14.2f}, {1.90f, -13.1f}, {1.98f, -12.0f},
        {2.15f, -10.3f}, {2.31f, -8.9f}, {2.48f, -7.7f}, {2.65f, -6.7f},
        {2.81f, -5.8f}, {2.98f, -5.0f}, {3.06f, -4.6f}, {3.48f, -3.1f},
        {3.98f, -1.6f}, {4.48f, -0.5f}, {4.98f, 0.f}, {5.31f, 0.f},
    };
    for (const auto& point : points)
        CHECK_NEAR(20.f * std::log10(v.gain(p, point[0])), point[1], 1e-4);
    CHECK_NEAR(v.gain(p, 10.f), 1.f, 0.f);
}

static void testFadeAndMute() {
    Vca902 v;
    Vca902::Params p;
    Vca902::Inputs in;
    in.in = 2.f;
    for (bool exp : {false, true}) {
        p.exp = exp;
        for (float cv : {-0.5f, -1.f, -6.f}) {
            in.cv[0] = cv;
            const auto o = v.process(kSampleRate, p, in);
            CHECK_NEAR(o.out, 0.f, 0.f);
            CHECK_NEAR(o.outNeg, 0.f, 0.f);
        }
    }
    p.exp = true;
    CHECK_NEAR(v.gain(p, 1.06f), 0.f, 0.f);
    CHECK_NEAR(v.gain(p, 1.08f), 0.f, 0.f);
    CHECK_NEAR(v.gain(p, 1.0801f), 0.f, 1e-8);
    float previous = 0.f;
    bool monotone = true;
    for (int i = 1081; i <= 5310; ++i) {
        const float gain = v.gain(p, i / 1000.f);
        monotone &= gain >= previous;
        previous = gain;
    }
    CHECK_NEAR(monotone, true, 0.f);
    const float atJoin = v.gain(p, 1.15f);
    CHECK_NEAR(atJoin - v.gain(p, 1.1499f), v.gain(p, 1.1501f) - atJoin, 2e-8);
}

static void testSignalPath() {
    Vca902 v;
    Vca902::Params p;
    Vca902::Inputs in;
    in.cv[0] = 2.f;
    in.cv[1] = 3.f;
    in.cv[2] = 1.f;
    in.in2 = 2.f;
    auto o = v.process(kSampleRate, p, in);
    CHECK_NEAR(o.out, -1.86f, 1e-6);  // bench 2026-11-22, test 4
    CHECK_NEAR(o.outNeg, -o.out, 0.f);

    // A constant differential voltage passes at half gain.
    in.cv[0] = 0.f;
    in.cv[1] = 0.68f + 0.5f / 0.56f - in.cv[2];  // bench 2026-11-23, linsweep_172245
    in.in = 6.f;
    for (int i = 0; i < 512; ++i)
        o = v.process(kSampleRate, p, in);
    CHECK_NEAR(o.out, 2.f, 1e-6);
    CHECK_NEAR(o.outNeg, -o.out, 0.f);
}

static void testSoftClip() {
    Vca902 v;
    Vca902::Params p;
    Vca902::Inputs in;
    p.exp = true;
    in.cv[0] = 4.98f;
    float previous = -10.f;
    bool monotonic = true;
    bool bounded = true;
    for (int signal = -100; signal <= 100; ++signal) {
        in.in = float(signal);
        const auto o = v.process(kSampleRate, p, in);
        bounded &= std::fabs(o.out) <= 10.f && std::fabs(o.outNeg) <= 10.f;
        monotonic &= o.out >= previous;
        previous = o.out;
    }
    CHECK_NEAR(bounded, true, 0.f);
    CHECK_NEAR(monotonic, true, 0.f);
    CHECK_NEAR(previous, 10.f, 1e-5);
    in.in = 10.f;
    CHECK_NEAR(v.process(kSampleRate, p, in).out, 9.5231883, 1e-5);  // 8 + 2·tanh(1)
    // Transparent below the 8 V knee: CV processing at normal levels is not distorted.
    in.in = 5.f;
    CHECK_NEAR(v.process(kSampleRate, p, in).out, 5.f, 1e-6);
    in.in = -7.9f;
    CHECK_NEAR(v.process(kSampleRate, p, in).out, -7.9f, 1e-6);
}

static void testKnobAndCvTiming() {
    Vca902 v;
    Vca902::Params p;
    p.exp = true;
    const Vca902::Params external{true, 0.f};
    for (float knob : {0.f, 0.5f, 1.f}) {
        p.fixedCv = knob;
        CHECK_NEAR(v.gain(p, 0.f), v.gain(external, 6.f * knob), 0.f);
    }

    Vca902::Inputs in;
    in.in = 1.f;
    p = {};
    CHECK_NEAR(v.process(kSampleRate, p, in).out, 0.f, 0.f);
    in.cv[0] = 6.f;
    CHECK_NEAR(v.process(kSampleRate, p, in).out, 0.93f, 1e-7);  // bench 2026-11-22, test 4
    in.cv[0] = 0.f;
    CHECK_NEAR(v.process(kSampleRate, p, in).out, 0.f, 0.f);

    p.fixedCv = 1.f;
    float out = v.process(kSampleRate, p, in).out;
    CHECK_NEAR(out == 0.f, true, 0.f);
    bool rising = true;
    for (int i = 1; i < 48; ++i) {
        const float next = v.process(kSampleRate, p, in).out;
        rising &= next >= out;
        out = next;
    }
    CHECK_NEAR(rising, true, 0.f);
    CHECK_NEAR(out, 0.93f, 1e-6);  // bench 2026-11-22, test 4
}

static void testSampleRates() {
    for (float sampleRate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        Vca902 v;
        Vca902::Params p;
        Vca902::Inputs in;
        bool finite = true;
        for (int i = 0; i < 512; ++i) {
            p.exp = i >= 256;
            p.fixedCv = (i % 64 < 32) ? 0.f : 1.f;
            in.cv[0] = float(i % 13 - 6);
            in.in = float(i % 23 - 11);
            in.in2 = 2.f;
            const auto o = v.process(sampleRate, p, in);
            finite &= std::isfinite(o.out) && std::isfinite(o.outNeg);
        }
        CHECK_NEAR(finite, true, 0.f);
    }
}

int main() {
    testGainLaws();
    testFadeAndMute();
    testSignalPath();
    testSoftClip();
    testKnobAndCvTiming();
    testSampleRates();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
