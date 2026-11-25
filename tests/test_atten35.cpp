// CP35 golden tests for the QSG half-normal routing and milestone 4 model.
#include "modules/Atten35.hpp"
#include "test_main.hpp"

using k280::Atten35;

static constexpr float kSampleRate = 48000.f;
static_assert(Atten35::kConfidence == k280::Confidence::Manual);

static void testHalfNormals() {
    Atten35 atten;
    const Atten35::Params p{{1.f, 1.f, 1.f, 1.f}};
    Atten35::Inputs in{{3.f, -4.f, 5.f, 7.f}};
    const auto unpatched = atten.process(kSampleRate, p, in);
    for (float voltage : unpatched.aout)
        CHECK_NEAR(voltage, 0.f, 0.f);

    in.ainConnected[0] = true;
    const auto first = atten.process(kSampleRate, p, in);
    for (float voltage : first.aout)
        CHECK_NEAR(voltage, 3.f * 0.564f, 1e-6);  // bench 2026-11-23, filtersweep_163544

    in.ainConnected[1] = true;
    const auto firstTwo = atten.process(kSampleRate, p, in);
    CHECK_NEAR(firstTwo.aout[0], 3.f * 0.564f, 1e-6);  // bench 2026-11-23, filtersweep_163544
    for (int channel = 1; channel < 4; ++channel)
        CHECK_NEAR(firstTwo.aout[channel], -4.f * 0.564f, 1e-6);  // bench 2026-11-23, filtersweep_163544

    in.ainConnected[0] = in.ainConnected[1] = false;
    in.ainConnected[2] = true;
    const auto third = atten.process(kSampleRate, p, in);
    CHECK_NEAR(third.aout[0], 0.f, 0.f);
    CHECK_NEAR(third.aout[1], 0.f, 0.f);
    CHECK_NEAR(third.aout[2], 5.f * 0.564f, 1e-6);  // bench 2026-11-23, filtersweep_163544
    CHECK_NEAR(third.aout[3], 5.f * 0.564f, 1e-6);  // bench 2026-11-23, filtersweep_163544

    in.ain[3] = 0.f;
    in.ainConnected[3] = true;
    const auto patchedZero = atten.process(kSampleRate, p, in);
    CHECK_NEAR(patchedZero.aout[2], 5.f * 0.564f, 1e-6);  // bench 2026-11-23, filtersweep_163544
    CHECK_NEAR(patchedZero.aout[3], 0.f, 0.f);
}

static void testTaperAndDc() {
    Atten35 atten;
    const float knobs[] = {0.f, 0.2f, 0.4f, 0.6f, 0.8f, 1.f};  // bench 2026-11-22, test 4
    const float ratios[] = {0.f, 0.079f, 0.202f, 0.333f, 0.443f, 0.564f};  // bench 2026-11-22, test 4
    Atten35::Inputs in{{6.f}, {true}};
    for (float sign : {-1.f, 1.f}) {
        in.ain[0] = sign * 6.f;
        for (int i = 0; i < 6; ++i) {
            const auto out = atten.process(kSampleRate, {{knobs[i]}}, in);
            CHECK_NEAR(out.aout[0] / in.ain[0], ratios[i], 1e-6);
        }
    }
}

static void testCalibration() {
    Atten35 atten;
    atten.cal.taperTable[2] = {0.45f, 0.25f};
    const Atten35::Params p{{0.45f, 0.525f}};
    const Atten35::Inputs in{{5.f}, {true}};
    const auto out = atten.process(kSampleRate, p, in);
    CHECK_NEAR(out.aout[0], 5.f * 0.25f, 1e-6);
    CHECK_NEAR(out.aout[1], 5.f * (0.25f + 0.333f) / 2.f, 1e-6);  // bench 2026-11-22, test 4
}

static void testReferencesMultiplesAndRates() {
    Atten35 atten;
    const Atten35::Params p{{1.f, 0.5f, 0.f, 1.f}};
    Atten35::Inputs in{{2.f, -4.f, 6.f, 8.f}, {true, true, true, true}};
    for (float rate : {0.f, 8000.f, 44100.f, 48000.f, 96000.f, 192000.f}) {
        in.maIn = in.mbIn = 0.f;
        const auto unpatched = atten.process(rate, p, in);
        CHECK_NEAR(unpatched.ma, 0.f, 0.f);
        CHECK_NEAR(unpatched.mb, 0.f, 0.f);
        in.maIn = 23.125f;
        in.mbIn = -19.25f;
        const auto out = atten.process(rate, p, in);
        CHECK_NEAR(out.ma, in.maIn, 0.f);
        CHECK_NEAR(out.mb, in.mbIn, 0.f);
        CHECK_NEAR(out.n6, -6.f, 0.f);
        CHECK_NEAR(out.p6, 6.f, 0.f);
        CHECK_NEAR(out.aout[0], 2.f * 0.564f, 1e-6);  // bench 2026-11-23, filtersweep_163544
        CHECK_NEAR(out.aout[1], -4.f * (0.202f + 0.333f) / 2.f, 1e-6);  // bench 2026-11-22, test 4
        CHECK_NEAR(out.aout[2], 0.f, 0.f);
        CHECK_NEAR(out.aout[3], 8.f * 0.564f, 1e-6);  // bench 2026-11-23, filtersweep_163544
    }
}

int main() {
    testHalfNormals();
    testTaperAndDc();
    testCalibration();
    testReferencesMultiplesAndRates();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
