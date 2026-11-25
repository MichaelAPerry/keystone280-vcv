// Golden tests for the 911 envelope, using the dial and RC assumptions in m3-specs.md.
#include "Knob.hpp"
#include "modules/Env911.hpp"
#include "test_main.hpp"

using k280::Env911;
using Stage = Env911::Stage;

static constexpr float kRate = 48000.f;
static constexpr Env911::Inputs kLow{0.f, true};
static constexpr Env911::Inputs kHigh{5.f, true};

// Bench 2026-11-23 refit through the CM1A 902 law.
static constexpr float kMinSeconds = .018f;  // Patch 1 live take and test 8
static constexpr float kNineSeconds = .5f;
static constexpr float kNoonSeconds = 1.2f;
static constexpr float kThreeSeconds = 18.f;
static constexpr float kMaxSeconds = 29.f;
static constexpr double kAttackMinSamples = .002 * kRate;
static constexpr int kAttackSamples = 100;
static constexpr double kReleaseMinSamples = double(kMinSeconds) * kRate;
static constexpr float kPeakV = 6.f;
static constexpr double kAttackTarget = 1.5 * kPeakV;

static void testKnobs() {
    Env911 env;
    const float knobs[] = {0.f, .2f, .5f, .8f, 1.f};
    const float times[] = {kMinSeconds, kNineSeconds, kNoonSeconds, kThreeSeconds, kMaxSeconds};
    const float attackTimes[] = {.002f, .47f, 1.4f, 15.f, 30.f};
    for (int i = 0; i < 5; ++i) {
        CHECK_NEAR(env.attackSeconds(knobs[i]), attackTimes[i], 1e-7);
        CHECK_NEAR(env.timeSeconds(knobs[i]), times[i], 1e-7);
        if (i > 0) {
            CHECK_NEAR(env.timeSeconds((knobs[i - 1] + knobs[i]) / 2),
                       std::sqrt(times[i - 1] * times[i]), times[i] * 5e-7);
        }
    }
    bool increasing = true;
    float previous = env.timeSeconds(0);
    for (int i = 1; i <= 1000; ++i) {
        const float time = env.timeSeconds(i / 1000.f);
        increasing &= time >= previous;
        previous = time;
    }
    CHECK_NEAR(increasing, true, 0);
    CHECK_NEAR(env.releaseSeconds(0), .018f, 0);
    CHECK_NEAR(env.releaseSeconds(.2f), .5f, 0);
    CHECK_NEAR(env.releaseSeconds(.5f), 1.2f, 0);
    CHECK_NEAR(env.releaseSeconds(.8f), 18.f, 0);
    CHECK_NEAR(env.releaseSeconds(1), 29.f, 0);
    CHECK_NEAR(env.sustainVolts(0), 0, 0);
    CHECK_NEAR(env.sustainVolts(.5f), 3.f, 0);
    CHECK_NEAR(env.sustainVolts(.8f), 5.55f, 0);
    CHECK_NEAR(env.sustainVolts(1), 6.f, 0);
}

static void testTrigger() {
    Env911 env;
    Env911::Params p;
    CHECK_NEAR(env.process(kRate, p, {0.f, false}).out, 0, 0);
    CHECK_NEAR(env.stage() == Stage::Idle, true, 0);
    CHECK_NEAR(env.process(kRate, p, kHigh).out, 0, 0);
    CHECK_NEAR(env.stage() == Stage::Idle, true, 0);
    // 5 V to 0 V crosses the 1 V threshold at 80% of the sample.
    CHECK_NEAR(env.process(kRate, p, kLow).out, kAttackTarget * -std::expm1(-std::log(3.) * .2 / kAttackMinSamples), 1e-7);
    CHECK_NEAR(env.stage() == Stage::Attack, true, 0);
    env.process(kRate, p, {1.5f, true});
    CHECK_NEAR(env.stage() == Stage::Attack, true, 0);
    env.process(kRate, p, kHigh);
    CHECK_NEAR(env.stage() == Stage::Release, true, 0);
    env.process(kRate, p, kLow);
    env.process(kRate, p, {0.f, false});
    CHECK_NEAR(env.stage() == Stage::Release, true, 0);
}

static void testAttack() {
    struct Case { float knob; double seconds; };
    const Case cases[] = {{0, .002}, {.2f, .47}, {.5f, 1.4}};
    for (const auto& c : cases) {
        Env911 env;
        Env911::Params p;
        p.t1 = c.knob;
        const int expected = std::lround(c.seconds * kRate);
        int samples = 0;
        float out = 0;
        while (env.stage() != Stage::Decay && samples < expected * 2) {
            out = env.process(kRate, p, kLow).out;
            ++samples;
            if (samples == expected / 2)
                CHECK_NEAR(out, kAttackTarget * -std::expm1(-std::log(3.) * samples / (c.seconds * kRate)), 1e-5);
        }
        CHECK_NEAR(env.stage() == Stage::Decay, true, 0);
        CHECK_NEAR(samples, expected, 1);
        CHECK_NEAR(out, kPeakV, 0);
    }
}

static void testDecaySustainRelease() {
    for (float knob : {0.f, .5f, 1.f}) {
        Env911 env;
        Env911::Params p;
        p.esus = knob;
        const double sustain = 6.0 * knob;
        for (int i = 0; i < kAttackSamples && env.stage() != Stage::Decay; ++i)
            env.process(kRate, p, kLow);

        float out = 0;
        for (int i = 0; i < std::lround(kReleaseMinSamples); ++i) out = env.process(kRate, p, kLow).out;
        CHECK_NEAR(out, sustain + (kPeakV - sustain) * .1, 1e-3);
        CHECK_NEAR(env.stage() == (knob == 1.f ? Stage::Sustain : Stage::Decay), true, 0);
        for (int i = 0; i < std::lround(5 * kReleaseMinSamples); ++i) out = env.process(kRate, p, kLow).out;
        CHECK_NEAR(env.stage() == Stage::Sustain, true, 0);
        CHECK_NEAR(out, sustain, 0);
        CHECK_NEAR(env.process(kRate, p, kLow).out, sustain, 0);

        for (int i = 0; i < std::lround(kReleaseMinSamples); ++i) out = env.process(kRate, p, kHigh).out;
        // The rising trigger reaches the 2 V release threshold 0.4 samples in.
        const double releaseSamples = std::lround(kReleaseMinSamples) - .4;
        CHECK_NEAR(out, sustain * std::pow(.1, releaseSamples / kReleaseMinSamples), 1e-4);
        for (int i = 0; i < std::lround(5 * kReleaseMinSamples); ++i) out = env.process(kRate, p, kHigh).out;
        CHECK_NEAR(out, 0, 0);
        CHECK_NEAR(env.stage() == Stage::Idle, true, 0);
    }
}

static void testMeasuredDecay() {
    const float knobs[] = {.2f, .5f, .8f};
    const float seconds[] = {kNineSeconds, kNoonSeconds, kThreeSeconds};
    for (int i = 0; i < 3; ++i) {
        Env911 env;
        Env911::Params p;
        p.t2 = knobs[i];
        for (int n = 0; n < kAttackSamples && env.stage() != Stage::Decay; ++n)
            env.process(kRate, p, kLow);
        CHECK_NEAR(env.stage() == Stage::Decay, true, 0);
        float out = 0.f;
        for (int n = 0; n < std::lround(seconds[i] * kRate); ++n)
            out = env.process(kRate, p, kLow).out;
        CHECK_NEAR(out, .1 * kPeakV, 1e-4);
    }
}

static void testReleaseCutoff() {
    Env911 env;
    Env911::Params p;
    p.esus = 1.f;
    for (int i = 0; i < kAttackSamples + std::lround(5 * kReleaseMinSamples); ++i) env.process(kRate, p, kLow);

    // Release crosses the trigger threshold 0.4 samples into the first interval.
    const double cutoffSamples = kReleaseMinSamples * std::log10(6. / .001);
    const int lastAboveCutoff = int(std::floor(cutoffSamples + .4));
    float out = 0;
    for (int i = 0; i < lastAboveCutoff; ++i) out = env.process(kRate, p, kHigh).out;
    CHECK_NEAR(out, 6. * std::pow(.1, (lastAboveCutoff - .4) / kReleaseMinSamples), 1e-8);
    CHECK_NEAR(env.stage() == Stage::Release, true, 0);
    CHECK_NEAR(env.process(kRate, p, kHigh).out, 0, 0);
    CHECK_NEAR(env.stage() == Stage::Idle, true, 0);
}

static void testInterruptedStages() {
    Env911 env;
    Env911::Params p;
    const double attackPerSample = std::log(3.) / kAttackMinSamples;
    const double releasePerSample = std::log(10.) / kReleaseMinSamples;
    float out = 0;
    for (int i = 0; i < 48; ++i) out = env.process(kRate, p, kLow).out;
    // Release crosses 2 V after 0.4 samples. Finish that part of attack first.
    const double atRelease = kAttackTarget - (kAttackTarget - out) * std::exp(-attackPerSample * .4);
    out = env.process(kRate, p, kHigh).out;
    CHECK_NEAR(out, atRelease * std::exp(-releasePerSample * .6), 1e-6);
    CHECK_NEAR(env.stage() == Stage::Release, true, 0);

    for (int i = 0; i < 20; ++i) out = env.process(kRate, p, kHigh).out;
    const double atRetrigger = out * std::exp(-releasePerSample * .8);
    out = env.process(kRate, p, kLow).out;
    CHECK_NEAR(out, kAttackTarget - (kAttackTarget - atRetrigger) * std::exp(-attackPerSample * .2), 1e-6);
    CHECK_NEAR(env.stage() == Stage::Attack, true, 0);
    for (int i = 0; i < kAttackSamples && env.stage() != Stage::Decay; ++i)
        out = env.process(kRate, p, kLow).out;
    CHECK_NEAR(out, kPeakV, 0);

    for (int i = 0; i < 480; ++i) out = env.process(kRate, p, kLow).out;
    const double atDecayRelease = out * std::pow(.1, .4 / kReleaseMinSamples);
    CHECK_NEAR(env.process(kRate, p, kHigh).out,
               atDecayRelease * std::exp(-releasePerSample * .6), 1e-6);
    CHECK_NEAR(env.stage() == Stage::Release, true, 0);
}

static void testBookPatch() {
    Env911::Params p;
    p.esus = k280::knobFromClock(9);
    CHECK_NEAR(p.esus, .2, 1e-7);
    for (float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        Env911 env;
        const int noteSamples = std::lround(.050 * rate);
        const int endSamples = noteSamples + std::lround(2 * kMinSeconds * rate);
        bool finite = true;
        bool bounded = true;
        float peak = 0, out = 0;
        for (int i = 0; i < endSamples; ++i) {
            out = env.process(rate, p, i < noteSamples ? kLow : kHigh).out;
            finite &= std::isfinite(out);
            bounded &= out >= 0 && out <= kPeakV;
            peak = std::max(peak, out);
        }
        CHECK_NEAR(finite, true, 0);
        CHECK_NEAR(bounded, true, 0);
        CHECK_NEAR(peak, kPeakV, 0);
        CHECK_NEAR(out < .1f, true, 0);
    }
}

int main() {
    testKnobs();
    testTrigger();
    testAttack();
    testDecaySustainRelease();
    testMeasuredDecay();
    testReleaseCutoff();
    testInterruptedStages();
    testBookPatch();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
