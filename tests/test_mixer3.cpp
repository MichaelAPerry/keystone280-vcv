// CP3A-M golden tests for the milestone 4 model and book patch 19.
#include "Knob.hpp"
#include "modules/Mixer3.hpp"
#include "test_main.hpp"

using k280::Mixer3;

static constexpr float kSampleRate = 48000.f;
static_assert(Mixer3::kConfidence == k280::Confidence::Manual);

static void testGainAndSum() {
    Mixer3 mixer;
    Mixer3::Params p{{1.f, 0.f, 0.f, 0.f}, 0.5f};
    Mixer3::Inputs in{{6.f, 10.f, 10.f, 10.f}};
    for (float master : {0.f, 0.25f, 0.5f, 0.75f, 1.f}) {
        p.master = master;
        const auto out = mixer.process(kSampleRate, p, in);
        CHECK_NEAR(out.outP, 6.f * master, 1e-6);  // bench 2026-11-22, test 12b
        CHECK_NEAR(out.outN, -out.outP, 0.f);
    }

    p = {{1.f, 0.5f, 0.25f, 0.75f}, 0.5f};
    in = {{1.f, -2.f, 4.f, 4.f}};
    for (int i = 0; i < 256; ++i) {
        const auto out = mixer.process(kSampleRate, p, in);
        CHECK_NEAR(out.outP, 2.f, 1e-6);  // bench 2026-11-22, test 12b
        CHECK_NEAR(out.outN, -2.f, 1e-6);  // bench 2026-11-22, test 12b
    }
}

static void testCalibration() {
    Mixer3 mixer;
    mixer.cal.gainTable[5] = {0.45f, 0.3f};
    mixer.cal.masterTable[5] = {0.55f, 0.9f};
    const Mixer3::Inputs in{{5.f}};
    CHECK_NEAR(mixer.process(kSampleRate, {{0.525f}, 0.55f}, in).outP, 2.025f, 1e-6);
    CHECK_NEAR(mixer.process(kSampleRate, {{0.45f}, 0.575f}, in).outP, 1.125f, 1e-6);  // bench 2026-11-22, test 12b
}

static void testSoftClip() {
    Mixer3 mixer;
    const Mixer3::Params p{{1.f}, 1.f};  // bench 2026-11-22, test 12b
    Mixer3::Inputs in;
    float previous = -8.f;
    for (int step = -400; step <= 400; ++step) {
        in.in[0] = step * 0.25f;
        const auto out = mixer.process(kSampleRate, p, in);
        CHECK_NEAR(out.outP >= previous && std::abs(out.outP) <= 8.f, true, 0.f);
        CHECK_NEAR(out.outN, -out.outP, 0.f);
        if (std::abs(in.in[0]) <= 6.5f)
            CHECK_NEAR(out.outP, in.in[0], 0.f);
        previous = out.outP;
    }
    CHECK_NEAR(previous, 8.f, 1e-6);
    for (float sign : {-1.f, 1.f}) {
        in.in[0] = sign * 8.f;
        CHECK_NEAR(mixer.process(kSampleRate, p, in).outP, sign * 7.6423912f, 1e-6);
    }
}

static void testMultiplesAndRates() {
    Mixer3 mixer;
    const Mixer3::Params p{{1.f, 1.f, 1.f, 1.f}, 1.f};
    Mixer3::Inputs in{{-20.f, -20.f, -20.f, -20.f}};
    for (float rate : {0.f, 8000.f, 44100.f, 48000.f, 96000.f, 192000.f}) {
        in.maIn = in.mbIn = 0.f;
        const auto unpatched = mixer.process(rate, p, in);
        CHECK_NEAR(unpatched.ma, 0.f, 0.f);
        CHECK_NEAR(unpatched.mb, 0.f, 0.f);
        in.maIn = -23.125f;
        in.mbIn = 19.25f;
        const auto patched = mixer.process(rate, p, in);
        CHECK_NEAR(patched.ma, in.maIn, 0.f);
        CHECK_NEAR(patched.mb, in.mbIn, 0.f);
        CHECK_NEAR(patched.outP, -8.f, 0.f);
        CHECK_NEAR(patched.outN, 8.f, 0.f);
    }
}

static void testPatch19() {
    Mixer3 mixer;
    const Mixer3::Params p{{k280::knobFromClock(14.f)}, k280::knobFromClock(12.f)};
    Mixer3::Inputs in;
    for (int step = 0; step <= 120; ++step) {
        const int level = step <= 60 ? step : 120 - step;
        in.in[0] = level / 10.f;
        const auto out = mixer.process(kSampleRate, p, in);
        CHECK_NEAR(out.outP, level * 0.035, 1e-6);  // bench 2026-11-22, test 12b
        CHECK_NEAR(out.outN, -level * 0.035, 1e-6);  // bench 2026-11-22, test 12b
    }
}

int main() {
    testGainAndSum();
    testCalibration();
    testSoftClip();
    testMultiplesAndRates();
    testPatch19();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
