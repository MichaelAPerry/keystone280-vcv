// Golden tests for the Behringer 961 Interface (sim/docs/m3-specs.md).
#include "modules/Filters923.hpp"
#include "modules/Interface961.hpp"
#include "modules/Lpf904A.hpp"
#include "modules/Seq960.hpp"
#include "test_main.hpp"

using k280::Interface961;

static constexpr float sampleRate = 48000.f;
static constexpr int minPulseSamples = 2160;  // bench 2026-11-22, test 8

static void testSwitchOnTime() {
    Interface961 v;
    CHECK_NEAR(v.switchOnSeconds(0.f), 0.045, 1e-7);  // bench 2026-11-22, test 8
    CHECK_NEAR(v.switchOnSeconds(0.5f), 0.275, 1e-7);  // bench 2026-11-22, test 8
    CHECK_NEAR(v.switchOnSeconds(0.8f), 3.0, 1e-7);  // bench 2026-11-22, test 8
    CHECK_NEAR(v.switchOnSeconds(1.f), 4.2, 1e-6);  // bench 2026-11-22, test 8
    CHECK_NEAR(v.switchOnSeconds(0.25f), std::sqrt(0.045 * 0.275), 1e-7);
}

static void testBPulse(int inputSamples, bool extraEdge) {
    Interface961 v;
    Interface961::Params p;
    Interface961::Inputs in;
    auto& bank = in.bank[0];
    bank.bConnected[0] = bank.bConnected[1] = true;
    CHECK_NEAR(v.process(sampleRate, p, in).bank[0].sOut, 12.f, 0);

    int lowSamples = 0;
    int fallingEdges = 0;
    float previous = 12.f;
    for (int i = 0; i < std::max(inputSamples, minPulseSamples) + 2; ++i) {
        bank.b[0] = i < inputSamples ? 5.f : 0.f;
        bank.b[1] = extraEdge && i >= minPulseSamples / 2 ? 5.f : 0.f;
        const float sOut = v.process(sampleRate, p, in).bank[0].sOut;
        if (i == 0) CHECK_NEAR(sOut, 0.f, 0);
        lowSamples += sOut == 0.f;
        fallingEdges += previous == 12.f && sOut == 0.f;
        previous = sOut;
    }
    CHECK_NEAR(previous, 12.f, 0);
    CHECK_NEAR(lowSamples, minPulseSamples, 1);
    CHECK_NEAR(fallingEdges, 1, 0);
}

static void testVTrigThresholds() {
    Interface961 v;
    Interface961::Params p;
    Interface961::Inputs in;
    auto& bank = in.bank[0];
    const auto step = [&] { return v.process(sampleRate, p, in).bank[0].sOut; };

    bank.a[2] = bank.b[2] = 5.f;
    CHECK_NEAR(step(), 12.f, 0);  // Ignore voltage on unpatched jacks.
    bank.a[2] = bank.b[2] = 0.f;
    bank.aConnected[2] = bank.bConnected[2] = true;
    CHECK_NEAR(step(), 12.f, 0);

    bank.a[2] = 1.4f;
    CHECK_NEAR(step(), 12.f, 0);
    bank.a[2] = 1.5f;
    CHECK_NEAR(step(), 0.f, 0);
    bank.a[2] = 1.f;
    CHECK_NEAR(step(), 0.f, 0);
    bank.a[2] = 0.99f;
    CHECK_NEAR(step(), 12.f, 0);

    bank.b[2] = 1.4f;
    CHECK_NEAR(step(), 12.f, 0);
    bank.b[2] = 0.f;
    CHECK_NEAR(step(), 12.f, 0);
    bank.b[2] = 1.6f;
    CHECK_NEAR(step(), 0.f, 0);
}

static void testBanksAndCombination() {
    Interface961 v;
    Interface961::Params p;
    Interface961::Inputs in;
    auto& left = in.bank[0];
    auto& right = in.bank[1];
    const auto step = [&] { return v.process(sampleRate, p, in); };

    left.aConnected[0] = left.bConnected[1] = true;
    right.bConnected[0] = true;
    left.a[0] = 5.f;
    auto out = step();
    CHECK_NEAR(out.bank[0].sOut, 0.f, 0);
    CHECK_NEAR(out.bank[1].sOut, 12.f, 0);
    CHECK_NEAR(v.bankActive(0), true, 0);
    CHECK_NEAR(v.bankActive(1), false, 0);

    int lowSamples = 0;
    for (int i = 0; i < 2 * minPulseSamples; ++i) {
        lowSamples += step().bank[0].sOut == 0.f;
    }
    CHECK_NEAR(lowSamples, 2 * minPulseSamples, 0);

    // Ignore a B edge while A holds this bank. The other bank can start its own pulse.
    left.b[1] = right.b[0] = 5.f;
    out = step();
    CHECK_NEAR(out.bank[0].sOut, 0.f, 0);
    CHECK_NEAR(out.bank[1].sOut, 0.f, 0);
    left.a[0] = 0.f;
    out = step();
    CHECK_NEAR(out.bank[0].sOut, 12.f, 0);
    CHECK_NEAR(out.bank[1].sOut, 0.f, 0);
    CHECK_NEAR(v.bankActive(0), false, 0);
    CHECK_NEAR(v.bankActive(1), true, 0);

    // A takes over during a B pulse and holds low beyond the timer's expiry.
    right.aConnected[0] = true;
    right.a[0] = 5.f;
    lowSamples = 0;
    for (int i = 0; i < 2 * minPulseSamples; ++i) {
        lowSamples += step().bank[1].sOut == 0.f;
    }
    CHECK_NEAR(lowSamples, 2 * minPulseSamples, 0);
    right.a[0] = 0.f;
    CHECK_NEAR(step().bank[1].sOut, 12.f, 0);
}

static void testSToV() {
    Interface961 v;
    Interface961::Inputs in;
    const Interface961::Params p;
    const auto step = [&] { return v.process(sampleRate, p, in); };

    auto out = step();
    CHECK_NEAR(out.bank[0].sv, 0.f, 0);
    CHECK_NEAR(out.bank[1].sv, 0.f, 0);
    in.bank[0].sInConnected = true;
    out = step();
    CHECK_NEAR(out.bank[0].sv, 5.f, 0);
    CHECK_NEAR(out.bank[0].sOut, 12.f, 0);
    CHECK_NEAR(out.bank[1].sv, 0.f, 0);
    in.bank[0].sIn = 12.f;
    in.bank[1].sInConnected = true;
    out = step();
    CHECK_NEAR(out.bank[0].sv, 0.f, 0);
    CHECK_NEAR(out.bank[1].sv, 5.f, 0);
    in.bank[1].sInConnected = false;
    CHECK_NEAR(step().bank[1].sv, 0.f, 0);
}

static void testAudioBursts(float rate, int toneHz, double amplitude) {
    constexpr double twoPi = 6.283185307179586;
    Interface961 v, insensitive;
    const Interface961::Params p{0.7f}, zeroSens;
    Interface961::Inputs in;
    in.audioConnected = true;
    for (int i = 0; i < int(rate * 0.03); ++i) v.process(rate, p, in);
    for (int burst = 0; burst < 2; ++burst) {
        int rises = 0;
        float previous = 0.f;
        bool insensitiveSilent = true;
        for (int i = 0; i < int(rate * 0.1); ++i) {
            in.audio = float(amplitude * std::sin(twoPi * toneHz * i / rate));
            const float avt = v.process(rate, p, in).avt;
            rises += previous == 0.f && avt == 5.f;
            previous = avt;
            insensitiveSilent &= insensitive.process(rate, zeroSens, in).avt == 0.f;
        }
        CHECK_NEAR(rises, 1, 0);
        CHECK_NEAR(previous, 5.f, 0);
        CHECK_NEAR(insensitiveSilent, true, 0);
        in.audio = 0.f;
        for (int i = 0; i < int(rate * 0.1); ++i)
            previous = v.process(rate, p, in).avt;
        CHECK_NEAR(previous, 0.f, 0);
    }
}

static void testAudioHysteresis() {
    Interface961 v;
    v.cal.audioHighPassHz = 0.f;
    v.cal.minAudioGain = v.cal.maxAudioGain = 1.f;
    const auto hold = [&](float volts) {
        float avt = 0.f;
        for (int i = 0; i < int(sampleRate * 0.05); ++i)
            avt = v.process(sampleRate, {}, {volts, true}).avt;
        return avt;
    };

    CHECK_NEAR(hold(0.75f), 0.f, 0);
    CHECK_NEAR(hold(1.5f), 5.f, 0);
    CHECK_NEAR(hold(0.75f), 5.f, 0);
    CHECK_NEAR(hold(0.5f), 0.f, 0);
    CHECK_NEAR(hold(-1.5f), 5.f, 0);
}

static void testAudioDisconnect() {
    Interface961 v;
    const Interface961::Params p{1.f};
    Interface961::Inputs in{0.f, true};
    const auto step = [&] { return v.process(sampleRate, p, in).avt; };
    float gate = 0.f;
    for (int i = 0; i < int(sampleRate * .03); ++i) step();
    in.audio = 5.f;
    for (int i = 0; i < int(sampleRate * .02); ++i) gate = step();
    CHECK_NEAR(gate, 5.f, 0);
    in.audioConnected = false;
    CHECK_NEAR(step(), 0.f, 0);

    in.audioConnected = true;
    for (int i = 0; i < int(sampleRate); ++i) gate = step();
    CHECK_NEAR(gate, 0.f, 0);
    in = {};
    bool stayedLow = true;
    for (int i = 0; i < int(sampleRate * .1); ++i) stayedLow &= step() == 0.f;
    CHECK_NEAR(stayedLow, true, 0);
}

static void testAudioNoise() {
    k280::Filters923 noise;
    k280::Lpf904A filter;
    const k280::Lpf904A::Params filterParams{0.5f, 1, 0.2f};  // bench 2026-11-22, test 8c
    const Interface961::Params p{0.5f};
    Interface961 directWhite, directPink, filtered;
    k280::Seq960 whiteSeq, pinkSeq, filteredSeq;
    const auto step = [](k280::Seq960& seq, float gate) {
        k280::Seq960::Inputs in;
        in.shiftConnected = true;
        in.shiftV = gate;
        const int previous = seq.stage();
        seq.process(sampleRate, {}, in);
        return previous != seq.stage();
    };

    int whiteSteps = 0, pinkSteps = 0, filteredSteps = 0;
    int lastEdge = -1, minGap = int(5 * sampleRate), maxGap = 0;
    for (int i = 0; i < int(5 * sampleRate); ++i) {
        const auto sources = noise.process(sampleRate, {}, {});
        const float low = filter.process(sampleRate, filterParams, {sources.white}).out;
        whiteSteps += step(whiteSeq, directWhite.process(sampleRate, p, {sources.white, true}).avt);
        pinkSteps += step(pinkSeq, directPink.process(sampleRate, p, {sources.pink, true}).avt);
        if (step(filteredSeq, filtered.process(sampleRate, p, {low, true}).avt)) {
            ++filteredSteps;
            if (lastEdge >= 0) {
                minGap = std::min(minGap, i - lastEdge);
                maxGap = std::max(maxGap, i - lastEdge);
            }
            lastEdge = i;
        }
    }
    CHECK_NEAR(whiteSteps, 0, 0);
    CHECK_NEAR(pinkSteps, 0, 0);
    CHECK_NEAR(filteredSteps >= 5, true, 0);

    // Dropping the input releases the latch. Restoring it gives one step.
    for (int i = 0; i < int(0.1f * sampleRate); ++i)
        whiteSteps += step(whiteSeq, directWhite.process(sampleRate, p, {0.f, true}).avt);
    for (int i = 0; i < int(0.5f * sampleRate); ++i) {
        const float white = noise.process(sampleRate, {}, {}).white;
        whiteSteps += step(whiteSeq, directWhite.process(sampleRate, p, {white, true}).avt);
    }
    CHECK_NEAR(whiteSteps, 1, 0);
    CHECK_NEAR(maxGap > 2 * minGap, true, 0);
}

static void testBookPatch1() {
    Interface961 v;
    Interface961::Params p;
    Interface961::Inputs in;
    p.time[0] = 0.05f;
    in.bank[0].bConnected[0] = true;
    constexpr int period = int(sampleRate / 5);
    const double expected = sampleRate * v.switchOnSeconds(0.05f);
    float previous = 12.f;
    for (int cycle = 0; cycle < 5; ++cycle) {
        int lowSamples = 0;
        int fallingEdges = 0;
        for (int i = 0; i < period; ++i) {
            in.bank[0].b[0] = i < period * 32 / 100 ? 5.f : 0.f;
            const float sOut = v.process(sampleRate, p, in).bank[0].sOut;
            if (i == 0) CHECK_NEAR(sOut, 0.f, 0);
            fallingEdges += previous == 12.f && sOut == 0.f;
            lowSamples += sOut == 0.f;
            previous = sOut;
        }
        CHECK_NEAR(fallingEdges, 1, 0);
        CHECK_NEAR(lowSamples, expected, 1);
        CHECK_NEAR(previous, 12.f, 0);
    }
}

// The lower three panel jacks of each column behave like the top three.
static void testLowerJacks() {
    constexpr float rate = 1000.f;
    for (int bank = 0; bank < 2; ++bank) {
        for (int jack = 0; jack < Interface961::kJacksPerColumn; ++jack) {
            Interface961 v;
            Interface961::Params p;
            p.time[bank] = 0.5f;
            Interface961::Inputs in;
            auto& input = in.bank[bank];
            input.aConnected[jack] = input.bConnected[jack] = true;
            input.a[jack] = 5.f;
            CHECK_NEAR(v.process(rate, p, in).bank[bank].sOut, 0.f, 0);
            input.a[jack] = 0.f;
            CHECK_NEAR(v.process(rate, p, in).bank[bank].sOut, 12.f, 0);
            int lowSamples = 0;
            for (int i = 0; i < 280; ++i) {
                input.b[jack] = i == 0 ? 5.f : 0.f;
                lowSamples += v.process(rate, p, in).bank[bank].sOut == 0.f;
            }
            CHECK_NEAR(lowSamples, 275, 1);  // bench 2026-11-22, test 9
        }
    }
}

int main() {
    testLowerJacks();
    testSwitchOnTime();
    testBPulse(48, false);     // 1 ms input
    testBPulse(96000, false);  // 2 s input
    testBPulse(48, true);      // Another B jack rises during the one-shot.
    testVTrigThresholds();
    testBanksAndCombination();
    testSToV();
    for (float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        testAudioBursts(rate, 100, 0.7);
        testAudioBursts(rate, 1000, 0.7);
    }
    testAudioHysteresis();
    testAudioDisconnect();
    testAudioNoise();
    testBookPatch1();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
