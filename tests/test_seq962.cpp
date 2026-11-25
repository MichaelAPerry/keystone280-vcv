// Golden tests for the 962 model in sim/docs/m4-specs.md.
#include <initializer_list>

#include "modules/Seq960.hpp"
#include "modules/Seq962.hpp"
#include "test_main.hpp"

using k280::Seq960;
using k280::Seq962;

constexpr float kSampleRate = 48000.f;

static void checkSample(Seq962& seq, const Seq962::Inputs& in, int stage, float signal) {
    auto out = seq.process(kSampleRate, in);
    CHECK_NEAR(seq.stage(), stage, 0);
    CHECK_NEAR(out.out, signal, 0);
    for (int i = 0; i < 3; ++i) CHECK_NEAR(out.tout[i], i + 1 == stage ? 5.f : 0.f, 0);
}

static void testCyclingAndRouting() {
    for (bool thirdPatched : {false, true}) {
        Seq962 seq;
        Seq962::Inputs in;
        in.sin[0] = -9.25f;
        in.sin[1] = 7.125f;
        in.sin[2] = 0.f;  // A patched zero still enables stage 3.
        in.sinConnected[0] = in.sinConnected[1] = true;
        in.sinConnected[2] = thirdPatched;
        in.shiftConnected = true;
        checkSample(seq, in, 1, -9.25f);
        for (int stage = 2; stage <= (thirdPatched ? 3 : 2); ++stage) {
            in.shiftV = 5.f;
            checkSample(seq, in, stage, stage == 2 ? 7.125f : 0.f);
            in.sin[stage - 1] = -0.125f;
            checkSample(seq, in, stage, -0.125f);
            in.shiftV = 0.f;
            checkSample(seq, in, stage, -0.125f);
        }
        in.shiftV = 5.f;
        checkSample(seq, in, 1, -9.25f);
        in.sinConnected[0] = false;
        checkSample(seq, in, 1, 0.f);
    }
}

static void testShiftThreshold() {
    Seq962 seq;
    Seq962::Inputs in;
    in.shiftV = 1.6f;
    checkSample(seq, in, 1, 0.f);
    in.shiftConnected = true;
    in.shiftV = 1.4f;
    checkSample(seq, in, 1, 0.f);
    in.shiftV = 1.6f;
    checkSample(seq, in, 2, 0.f);
    in.shiftV = 0.5f;
    checkSample(seq, in, 2, 0.f);
    in.shiftV = 1.6f;
    checkSample(seq, in, 2, 0.f);
    in.shiftV = 0.4f;
    checkSample(seq, in, 2, 0.f);
    in.shiftV = 1.5f;
    checkSample(seq, in, 1, 0.f);
    in.shiftConnected = false;
    checkSample(seq, in, 1, 0.f);
    in.shiftConnected = true;
    checkSample(seq, in, 2, 0.f);
}

static void testDirectSelection() {
    for (int stage = 1; stage <= 3; ++stage) {
        Seq962 seq;
        Seq962::Inputs in;
        int other = stage % 3 + 1;
        int index = stage - 1;
        seq.restoreStage(other);
        in.sin[index] = 8.f;
        in.tin[index] = 5.f;
        checkSample(seq, in, other, 0.f);
        in.tinConnected[index] = true;
        in.tin[index] = 3.4f;
        checkSample(seq, in, other, 0.f);
        in.tin[index] = 3.5f;
        checkSample(seq, in, stage, 0.f);

        in.btn[other - 1] = true;
        checkSample(seq, in, other, 0.f);
        in.tin[index] = 2.5f;
        checkSample(seq, in, other, 0.f);
        in.tin[index] = 5.f;
        checkSample(seq, in, other, 0.f);
        in.tin[index] = 2.4f;
        checkSample(seq, in, other, 0.f);
        in.tin[index] = 5.f;
        checkSample(seq, in, stage, 0.f);

        in.btn[index] = true;
        checkSample(seq, in, stage, 0.f);
        in.shiftConnected = true;
        in.shiftV = 5.f;
        checkSample(seq, in, stage == 1 ? 2 : 1, 0.f);
        in.btn[index] = false;
        in.shiftV = 0.f;
        checkSample(seq, in, stage == 1 ? 2 : 1, 0.f);
        in.btn[index] = true;
        checkSample(seq, in, stage, 0.f);
        in.shiftV = 5.f;
        checkSample(seq, in, stage == 1 ? 2 : 1, 0.f);
    }
}

static void testSimultaneousEdges() {
    Seq962 seq;
    Seq962::Inputs in;
    in.shiftConnected = true;
    in.shiftV = 5.f;
    in.btn[2] = true;
    in.tinConnected[0] = true;
    in.tin[0] = 5.f;
    checkSample(seq, in, 1, 0.f);
    in.tin[0] = 0.f;
    checkSample(seq, in, 1, 0.f);
    in.shiftV = 0.f;
    checkSample(seq, in, 1, 0.f);
    in.shiftV = 5.f;
    checkSample(seq, in, 2, 0.f);
}

static void testPersistence() {
    Seq962 seq;
    for (int stage = 1; stage <= 3; ++stage) {
        seq.restoreStage(stage);
        CHECK_NEAR(seq.saveStage(), stage, 0);
        checkSample(seq, {}, stage, 0.f);
    }
    seq.restoreStage(-1);
    CHECK_NEAR(seq.stage(), 1, 0);
    seq.restoreStage(4);
    CHECK_NEAR(seq.stage(), 3, 0);
    seq.cal.shiftOnV = 4.f;
    Seq962::Inputs in;
    in.shiftConnected = true;
    in.shiftV = 4.f;
    checkSample(seq, in, 1, 0.f);
    seq.reset();
    CHECK_NEAR(seq.saveStage(), 1, 0);
    CHECK_NEAR(seq.cal.shiftOnV, 4, 0);
    checkSample(seq, in, 2, 0.f);
    seq.restoreStage(3);
    checkSample(seq, in, 1, 0.f);
}

static void testAudioRateShift() {
    Seq962 seq;
    Seq962::Inputs in;
    in.shiftConnected = in.sinConnected[2] = true;
    int wrongSamples = 0;
    for (int pulse = 0; pulse < 1000; ++pulse) {
        for (int sample = 0; sample < 48; ++sample) {
            in.shiftV = sample < 24 ? 5.f : 0.f;
            seq.process(kSampleRate, in);
            wrongSamples += seq.stage() != (pulse + 1) % 3 + 1;
        }
    }
    CHECK_NEAR(wrongSamples, 0, 0);
}

static void testBookPatch19() {
    Seq960 controller;
    Seq960::Params params;
    Seq960::Inputs clock;
    Seq962 seq;
    Seq962::Inputs in;
    in.shiftConnected = true;
    in.sinConnected[0] = in.sinConnected[1] = true;
    in.sin[0] = -2.f;
    in.sin[1] = 3.f;
    // Clock the 960 through four loops and feed its actual stage-1 gate to SHIFT.
    for (int loop = 0; loop < 4; ++loop) {
        for (int stage = 1; stage <= 8; ++stage) {
            for (int sample = 0; sample < 16; ++sample) {
                clock.shift = sample == 0 && (loop != 0 || stage != 1);
                auto source = controller.process(kSampleRate, params, clock);
                CHECK_NEAR(source.sout[0], stage == 1 ? 5.f : 0.f, 0);
                in.shiftV = source.sout[0];
                auto out = seq.process(kSampleRate, in);
                CHECK_NEAR(seq.stage(), loop % 2 == 0 ? 2 : 1, 0);
                CHECK_NEAR(out.out, loop % 2 == 0 ? 3.f : -2.f, 0);
            }
        }
    }
}

static void testSampleRates() {
    for (float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        Seq962 seq;
        Seq962::Inputs in;
        in.shiftConnected = true;
        for (bool& connected : in.sinConnected) connected = true;
        bool finite = true;
        for (int sample = 0; sample < 1024; ++sample) {
            in.shiftV = sample % 16 < 8 ? 5.f : 0.f;
            for (int i = 0; i < 3; ++i) in.sin[i] = float((sample + i) % 21 - 10);
            auto out = seq.process(rate, in);
            finite &= std::isfinite(out.out);
            for (float gate : out.tout) finite &= std::isfinite(gate);
        }
        CHECK_NEAR(finite, true, 0);
    }
}

int main() {
    testCyclingAndRouting();
    testShiftThreshold();
    testDirectSelection();
    testSimultaneousEdges();
    testPersistence();
    testAudioRateShift();
    testBookPatch19();
    testSampleRates();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
