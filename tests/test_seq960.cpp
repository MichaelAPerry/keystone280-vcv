// Golden tests for the 960 model in sim/docs/m3-specs.md.
#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "modules/Seq960.hpp"
#include "test_main.hpp"

using k280::Seq960;

constexpr int kSampleRate = 48000;

static void checkStage(const Seq960& seq, const Seq960::Outputs& out, int expected) {
    CHECK_NEAR(seq.stage(), expected, 0);
    for (int i = 0; i < 9; ++i) CHECK_NEAR(out.sout[i], i + 1 == expected ? 5 : 0, 0);
}

static void shift(Seq960& seq, const Seq960::Params& p) {
    seq.process(kSampleRate, p, {});
    Seq960::Inputs in;
    in.shift = true;
    seq.process(kSampleRate, p, in);
}

static void testFrequency() {
    Seq960 seq;
    Seq960::Params p;
    const double lo[] = {0.04, 2.75, 0.17, 11, 0.7, 44};
    const double hi[] = {0.5, 30, 2, 130, 8, 500};
    for (int i = 0; i < 6; ++i) {
        p.freqRange = i;
        p.vernier = 0;
        CHECK_NEAR(seq.oscHz(p, 0), lo[i], 1e-6);
        p.vernier = 1;
        CHECK_NEAR(seq.oscHz(p, 0), hi[i], 1e-6);
    }
    p.vernier = std::log(100.0 / 44) / std::log(500.0 / 44);
    CHECK_NEAR(seq.oscHz(p, 0), 100, 1e-4);
    CHECK_NEAR(seq.oscHz(p, 2), 400, 1e-4);
    CHECK_NEAR(seq.oscHz(p, -2), 25, 1e-4);
    CHECK_NEAR(seq.oscHz(p, 8), 3200, 1e-3);
    CHECK_NEAR(seq.oscHz(p, -8), 3.125, 1e-5);
}

static void testRowsAndPersistence() {
    Seq960 seq;
    Seq960::Params p;
    CHECK_NEAR(seq.oscRunning(), false, 0);
    checkStage(seq, seq.process(kSampleRate, p, {}), 1);
    for (int i = 0; i < 8; ++i) {
        seq.restoreStage(i + 1);
        CHECK_NEAR(seq.saveStage(), i + 1, 0);
        for (int row = 0; row < 3; ++row) {
            p.rowRange[row] = (i + row) % 3;
            p.knobs[row][i] = (i + row) / 9.f;
        }
        auto out = seq.process(kSampleRate, p, {});
        checkStage(seq, out, i + 1);
        for (int row = 0; row < 3; ++row) {
            CHECK_NEAR(out.row[row], (i + row) / 9.0 * (2 << p.rowRange[row]), 1e-6);
        }
    }
    seq.restoreStage(9);
    checkStage(seq, seq.process(kSampleRate, p, {}), 9);
    seq.reset();
    CHECK_NEAR(seq.saveStage(), 1, 0);
    CHECK_NEAR(seq.oscRunning(), false, 0);
}

static void testClock() {
    Seq960 seq;
    Seq960::Params p;
    p.freqRange = 1;
    p.vernier = std::log(10.25 / 2.75) / std::log(30.0 / 2.75);
    Seq960::Inputs in;
    in.oscOn = true;
    in.oscCv = 5.f;  // unpatched
    auto out = seq.process(kSampleRate, p, in);
    CHECK_NEAR(seq.oscRunning(), true, 0);
    CHECK_NEAR(seq.stage(), 1, 0);
    CHECK_NEAR(out.oscOut, 5, 0);
    int previousStage = 1;
    float previousV = out.oscOut;
    int steps = 0, high = 0, mistimed = 0, invalidVoltage = 0, firstStep = 0;
    for (int sample = 1; sample <= 2 * kSampleRate; ++sample) {
        // Hold OSC ON throughout. Its rising edge is the only start event.
        out = seq.process(kSampleRate, p, in);
        bool stepped = seq.stage() != previousStage;
        if (stepped && steps++ == 0) firstStep = sample;
        mistimed += stepped != (previousV == 0.f && out.oscOut == 5.f);
        high += out.oscOut == 5.f;
        invalidVoltage += out.oscOut != 0.f && out.oscOut != 5.f;
        previousV = out.oscOut;
        previousStage = seq.stage();
    }
    CHECK_NEAR(steps, 20, 0);
    CHECK_NEAR(firstStep, kSampleRate / 10.25, 1);
    CHECK_NEAR(double(high) / (2 * kSampleRate), 0.32, 0.01);
    CHECK_NEAR(mistimed, 0, 0);
    CHECK_NEAR(invalidVoltage, 0, 0);
    in.oscOff = true;
    const int stopped = seq.stage();
    for (int i = 0; i < kSampleRate / 2; ++i) out = seq.process(kSampleRate, p, in);
    CHECK_NEAR(seq.oscRunning(), false, 0);
    CHECK_NEAR(seq.stage(), stopped, 0);
    CHECK_NEAR(out.oscOut, 0, 0);
}

static void testBenchDuty() {
    const double periods[] = {6.7, 3.1, 2.25};  // bench 2026-11-22, test 8
    const double duties[] = {0.31, 0.32, 0.33};  // bench 2026-11-22, test 8
    constexpr int rate = 10000;
    for (int i = 0; i < 3; ++i) {
        Seq960 seq;
        Seq960::Params p;
        seq.cal.hzLo[0] = seq.cal.hzHi[0] = 1 / periods[i];
        Seq960::Inputs in;
        in.oscOn = true;
        auto out = seq.process(rate, p, in);
        int samples = 0, high = 0;
        while (seq.stage() == 1 && samples < int(periods[i] * rate) + 2) {
            high += out.oscOut > 0.f;
            ++samples;
            out = seq.process(rate, p, {});
        }
        CHECK_NEAR(seq.stage(), 2, 0);
        CHECK_NEAR(double(high) / samples, duties[i], 0.015);
    }
}

static void testAdvance() {
    Seq960 seq;
    Seq960::Params p;
    p.mode[1] = p.mode[2] = 0;
    shift(seq, p);
    CHECK_NEAR(seq.stage(), 4, 0);
    seq.restoreStage(8);
    shift(seq, p);
    CHECK_NEAR(seq.stage(), 1, 0);

    p.mode9 = 1;
    p.freqRange = 5;
    p.vernier = 1;
    seq.restoreStage(8);
    Seq960::Inputs in;
    in.oscOn = true;
    seq.process(kSampleRate, p, in);
    CHECK_NEAR(seq.oscRunning(), true, 0);
    Seq960::Outputs out;
    for (int i = 0; i < 100; ++i) out = seq.process(kSampleRate, p, {});
    checkStage(seq, out, 9);
    for (float row : out.row) CHECK_NEAR(row, 0, 0);
    CHECK_NEAR(out.oscOut, 0, 0);
    CHECK_NEAR(seq.oscRunning(), false, 0);

    p.mode9 = 0;
    for (int& mode : p.mode) mode = 0;
    seq.reset();
    shift(seq, p);
    checkStage(seq, seq.process(kSampleRate, p, {}), 9);
}

static void testStop() {
    for (auto stopMode : {Seq960::StopMode::OscOnly, Seq960::StopMode::BlockShift}) {
        Seq960 seq;
        Seq960::Params p;
        p.stopMode = stopMode;
        p.freqRange = 5;
        p.vernier = 1;
        p.mode[1] = 2;
        p.knobs[0][1] = 0.75f;
        Seq960::Inputs in;
        in.oscOn = true;
        seq.process(kSampleRate, p, in);
        Seq960::Outputs out;
        for (int i = 0; i < 400; ++i) out = seq.process(kSampleRate, p, {});
        checkStage(seq, out, 2);
        CHECK_NEAR(out.row[0], 1.5, 0);
        CHECK_NEAR(seq.oscRunning(), false, 0);
        shift(seq, p);
        CHECK_NEAR(seq.stage(), stopMode == Seq960::StopMode::OscOnly ? 3 : 2, 0);

        seq.restoreStage(2);
        in = {};
        in.set[3] = true;
        checkStage(seq, seq.process(kSampleRate, p, in), 4);
        seq.restoreStage(2);
        in = {};
        in.sinConnected[4] = true;
        in.sin[4] = 5.f;
        checkStage(seq, seq.process(kSampleRate, p, in), 5);

        seq.restoreStage(2);
        in = {};
        in.oscOn = true;
        checkStage(seq, seq.process(kSampleRate, p, in), 2);
        CHECK_NEAR(seq.oscRunning(), true, 0);
        for (int i = 0; i < 100; ++i) out = seq.process(kSampleRate, p, {});
        checkStage(seq, out, 3);
    }
}

static void testTriggers() {
    Seq960 seq;
    Seq960::Params p;
    Seq960::Inputs in;
    in.shiftV = 1.6f;
    checkStage(seq, seq.process(kSampleRate, p, in), 1);
    // rack 2026-11-22: a 921 RECT (-0.525 / +0.571 V) steps SHIFT.
    in.shiftConnected = true;
    in.shiftV = 0.3f;
    checkStage(seq, seq.process(kSampleRate, p, in), 1);
    in.shiftV = 0.55f;
    checkStage(seq, seq.process(kSampleRate, p, in), 2);
    in.shiftV = 0.1f;  // still above release (0.05 V): no rearm
    seq.process(kSampleRate, p, in);
    in.shiftV = 0.55f;
    checkStage(seq, seq.process(kSampleRate, p, in), 2);
    in.shiftV = -0.525f;
    seq.process(kSampleRate, p, in);
    in.shiftV = 0.571f;
    checkStage(seq, seq.process(kSampleRate, p, in), 3);

    p.mode[5] = 0;
    in = {};
    in.sinConnected[5] = true;
    in.sin[5] = 3.4f;
    checkStage(seq, seq.process(kSampleRate, p, in), 3);
    in.sin[5] = 3.5f;
    checkStage(seq, seq.process(kSampleRate, p, in), 6);
    in.shift = true;
    checkStage(seq, seq.process(kSampleRate, p, in), 7);
    checkStage(seq, seq.process(kSampleRate, p, in), 7);
    in = {};
    in.set[5] = true;
    checkStage(seq, seq.process(kSampleRate, p, in), 6);
    in.shift = true;
    checkStage(seq, seq.process(kSampleRate, p, in), 7);

    in = {};
    in.oscOnV = in.oscOffV = 3.5f;
    in.oscOnConnected = true;
    seq.process(kSampleRate, p, in);
    CHECK_NEAR(seq.oscRunning(), true, 0);
    in.oscOffConnected = true;
    seq.process(kSampleRate, p, in);
    CHECK_NEAR(seq.oscRunning(), false, 0);
    seq.process(kSampleRate, p, in);
    CHECK_NEAR(seq.oscRunning(), false, 0);

    in = {};
    in.oscOn = true;
    seq.process(kSampleRate, p, in);
    in.set[8] = true;
    checkStage(seq, seq.process(kSampleRate, p, in), 9);
    CHECK_NEAR(seq.oscRunning(), false, 0);
}

static void testAudioRateShift() {
    Seq960 seq;
    Seq960::Params p;
    Seq960::Inputs in;
    in.shiftConnected = true;
    int wrongSamples = 0;
    for (int pulse = 0; pulse < 1000; ++pulse) {
        for (int sample = 0; sample < 48; ++sample) {
            in.shiftV = sample < 24 ? 5.f : 0.f;
            seq.process(kSampleRate, p, in);
            wrongSamples += seq.stage() != (pulse + 1) % 8 + 1;
        }
    }
    CHECK_NEAR(wrongSamples, 0, 0);
}

static void testTiming() {
    struct Case { double base; int range; float knob; double ratio; };
    const Case cases[] = {
        {0.621, 0, 0.f, 2.04}, {0.621, 0, 1.f, 0.77},  // bench 2026-11-22, test 11
        {0.606, 0, 0.2f, 1.34},  // bench 2026-11-22, test 11b
        {0.606, 0, 0.5f, 1.0},   // bench 2026-11-22, test 11b
        {0.606, 0, 0.8f, 0.82},  // bench 2026-11-22, test 11b
        {0.606, 1, 1.f, 0.83},   // bench 2026-11-22, test 11b
        {0.606, 2, 0.f, 2.0},    // bench 2026-11-22, test 11b
        {0.606, 2, 0.5f, 0.83},  // bench 2026-11-22, test 11b
        {0.606, 2, 1.f, 0.89},   // bench 2026-11-22, test 11b
        {2.265, 0, 0.f, 2.15}, {2.265, 0, 1.f, 0.497},  // bench 2026-11-22, test 11c
        {0.140, 0, 0.f, 2.17}, {0.140, 0, 1.f, 0.84},   // bench 2026-11-22, test 11d
    };
    constexpr int rate = 10000;
    for (const auto& c : cases) {
        Seq960 seq;
        Seq960::Params p;
        seq.cal.hzLo[0] = seq.cal.hzHi[0] = 1 / c.base;
        p.timing = true;
        p.rowRange[2] = c.range;
        p.knobs[2][0] = c.knob;
        Seq960::Inputs in;
        in.oscOn = true;
        CHECK_NEAR(seq.process(rate, p, in).row[2], c.knob * (2 << c.range), 1e-6);
        int samples = 0;
        while (seq.stage() == 1 && samples < 3 * c.base * rate) {
            seq.process(rate, p, {});
            ++samples;
        }
        CHECK_NEAR(seq.stage(), 2, 0);
        CHECK_NEAR(samples / (rate * c.base), c.ratio, 0.15 * c.ratio);
    }

    Seq960 seq;
    Seq960::Params p;
    p.knobs[2][0] = 0.9f;
    p.knobs[2][1] = 1.f;
    const double baseHz = 1 / 0.606;  // bench 2026-11-22, test 11b
    CHECK_NEAR(seq.stageHz(p, 1, baseHz), baseHz, 0);
    p.timing = true;
    CHECK_NEAR(seq.stageHz(p, 1, baseHz), seq.stageHz(p, 2, baseHz), 1e-9);
}

static void testSampleRates() {
    for (float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        Seq960 seq;
        Seq960::Params p;
        p.freqRange = 5;
        p.vernier = 1;
        p.timing = true;
        Seq960::Inputs in;
        in.oscOn = true;
        in.oscCvConnected = true;
        in.oscCv = 5.f;
        bool finite = true;
        for (int sample = 0; sample < 1024; ++sample) {
            auto out = seq.process(rate, p, in);
            in.oscOn = false;
            finite &= std::isfinite(out.oscOut);
            for (float row : out.row) finite &= std::isfinite(row);
            for (float gate : out.sout) finite &= std::isfinite(gate);
        }
        CHECK_NEAR(finite, true, 0);
    }
}

static void testSlowClock() {
    Seq960 seq;
    Seq960::Params p;
    p.vernier = 0;
    Seq960::Inputs in;
    in.oscOn = true;
    seq.process(kSampleRate, p, in);
    const int period = 25 * kSampleRate;
    int elapsed = 0;
    while (seq.stage() == 1 && elapsed < period + 2) {
        seq.process(kSampleRate, p, {});
        ++elapsed;
    }
    CHECK_NEAR(seq.stage(), 2, 0);
    CHECK_NEAR(elapsed, period, 1);
}

int main() {
    testFrequency();
    testRowsAndPersistence();
    testClock();
    testBenchDuty();
    testAdvance();
    testStop();
    testTriggers();
    testAudioRateShift();
    testTiming();
    testSampleRates();
    testSlowClock();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
