// Golden tests for the Abacus model in m5-specs.md.
#include "Knob.hpp"
#include "modules/Abacus.hpp"
#include "test_main.hpp"

using k280::Abacus;
using Stage = Abacus::Stage;

static constexpr float kRate = 48000;
static constexpr float kEorHigh = 8.7f;  // bench 2026-11-22, test 14
static constexpr float kEocHigh = 9.8f;  // bench 2026-11-22, test 16
static constexpr double kNineSamples = .0016 * kRate;  // bench 2026-11-22, test 15a
static constexpr double kTenSamples = .0042 * kRate;  // bench 2026-11-22, test 15a
static constexpr double kEightSamples = .0009 * kRate;  // bench 2026-11-22, test 15a (interpolated)

static void testTimeTable() {
    Abacus a;
    const double times[] = {
        .0005,
        .0009,  // bench 2026-11-22, test 15a (interpolated)
        .0016, .0042,  // bench 2026-11-22, test 15a
        .0144,  // bench 2026-11-22, test 15a
        .05, .14, .44, 3.1,  // bench 2026-11-22, test 15a/15b
        9, 25,  // bench 2026-11-22, test 15b
    };
    for (bool fall : {false, true}) {
        for (int i = 0; i < 10; ++i) {
            CHECK_NEAR(a.timeSeconds(i / 10.f, .5f, fall), times[i], times[i] * 1e-6);
            const double midpoint = std::sqrt(times[i] * times[i + 1]);
            CHECK_NEAR(a.timeSeconds((i + .5f) / 10.f, .5f, fall), midpoint, midpoint * 1e-6);
        }
        CHECK_NEAR(a.timeSeconds(1, .5f, fall), times[10], 0);
    }
    const double fallBench[] = {.017, .048, .13, .41, 2.8};  // bench 2026-11-22, test 15b
    for (int i = 0; i < 5; ++i) {
        // At 11 o'clock the shared taper is fitted to the measured cycle period.
        const double tolerance = i == 0 ? .003 : .15 * fallBench[i];  // bench 2026-11-22, test 15a/15b
        CHECK_NEAR(a.timeSeconds((i + 4) / 10.f, .5f, true), fallBench[i], tolerance);
    }
    CHECK_NEAR(a.timeSeconds(-1, .5f, false), times[0], 1e-10);
    CHECK_NEAR(a.timeSeconds(2, .5f, true), times[10], 0);
    CHECK_NEAR(a.timeSeconds(1, 0, false), 540, 0);
    CHECK_NEAR(a.timeSeconds(1, 0, true), 1515, 0);
    CHECK_NEAR(a.timeSeconds(1, 1, false), 7, 0);
    CHECK_NEAR(a.timeSeconds(1, 1, true), 5, 0);
    CHECK_NEAR(a.timeSeconds(.5f, .25f, false), times[5] * std::sqrt(540. / times[10]), 1e-6);
    CHECK_NEAR(a.timeSeconds(.5f, .75f, true), times[5] * std::sqrt(5. / times[10]), 1e-6);
}

static void testTriggerAndRetrigger() {
    Abacus a;
    Abacus::Params p;
    Abacus::Inputs in;
    p.rise1 = p.fall1 = .4f;
    const double duration = .0144 * kRate;  // bench 2026-11-22, test 15a
    in.trig1 = 5;
    a.process(kRate, p, in);
    CHECK_NEAR(a.stage(1) == Stage::Idle, true, 0);
    in.trig1Connected = true;
    in.trig1 = 1.f;  // bench 2026-11-22, test 14
    a.process(kRate, p, in);
    CHECK_NEAR(a.stage(1) == Stage::Idle, true, 0);

    in.trig1 = 0;
    for (int i = 0; i < kRate / 100; ++i) a.process(kRate, p, in);
    in.trig1 = 1.4f;  // bench 2026-11-22, test 14
    a.process(kRate, p, in);
    CHECK_NEAR(a.stage(1) == Stage::Rise, true, 0);

    // A new edge during the rise must leave its slope intact.
    in.trig1 = 0;
    a.process(kRate, p, in);
    const double rising = a.process(kRate, p, in).unity1;
    in.trig1 = 5;
    CHECK_NEAR(a.process(kRate, p, in).unity1, rising + 10 / duration, 1e-6);
    while (a.stage(1) == Stage::Rise) a.process(kRate, p, in);

    in.trig1 = .65f;  // bench 2026-11-22, test 14 (above release)
    for (int i = 0; i < 200; ++i) a.process(kRate, p, in);
    in.trig1 = 1.4f;  // Above threshold, with insufficient voltage change.
    a.process(kRate, p, in);
    CHECK_NEAR(a.stage(1) == Stage::Fall, true, 0);

    in.trig1 = .55f;  // bench 2026-11-22, test 14 (below release)
    const double before = a.process(kRate, p, in).unity1;
    in.trig1 = 5;
    const double fraction = (1.1 - .55) / (5 - .55);  // bench 2026-11-22, test 14
    const double atEdge = before - 10 * fraction / duration;
    const auto out = a.process(kRate, p, in);
    CHECK_NEAR(a.stage(1) == Stage::Rise, true, 0);
    CHECK_NEAR(out.unity1, atEdge + (10 - atEdge) * (1 - fraction) / duration, 1e-6);
    CHECK_NEAR(out.eor, 0, 0);
}

static void testTriggerRamps() {
    // bench 2026-11-22, test 14
    for (float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        Abacus a;
        Abacus::Inputs in;
        in.trig1Connected = true;
        int triggers = 0;
        const auto step = [&](float volts) {
            in.trig1 = volts;
            const auto previous = a.stage(1);
            a.process(rate, {}, in);
            triggers += previous != Stage::Rise && a.stage(1) == Stage::Rise;
        };
        const auto ramp = [&](float volts, double seconds) {
            const double samples = seconds * rate;
            for (int i = 0; i <= std::ceil(samples); ++i)
                step(volts * std::min(1.0, i / samples));
        };
        const auto hold = [&](float volts) {
            for (int i = 0; i < rate / 100; ++i) step(volts);
        };

        ramp(10.f, 1);
        CHECK_NEAR(triggers, 0, 0);

        a = {};
        ramp(1.2f, 1);
        hold(1.2f);
        CHECK_NEAR(triggers, 0, 0);
        hold(5.f);
        CHECK_NEAR(triggers, 1, 0);

        a = {};
        triggers = 0;
        ramp(1.15f, .00025);
        CHECK_NEAR(triggers, 1, 0);
        hold(1.15f);
        CHECK_NEAR(triggers, 1, 0);

        for (int i = 0; i < 5; ++i) {
            for (int n = 0; n < rate / 1000; ++n) step(0);
            for (int n = 0; n < rate / 1000; ++n) step(1.4f);
        }
        CHECK_NEAR(triggers, 6, 0);
    }
}

struct SegmentSamples { int rise = 0, fall = 0; };

static SegmentSamples measureEnvelope(const Abacus::Params& p, Abacus::Inputs in, bool checkOutputs = false) {
    Abacus a;
    in.trig1Connected = in.trig4Connected = true;
    in.trig1 = in.trig4 = 5;
    CHECK_NEAR(a.process(kRate, p, {}).eoc, kEocHigh, 0);
    SegmentSamples n;
    Abacus::Outputs o;
    bool gatesCorrect = true;
    do {
        o = a.process(kRate, p, in);
        ++n.rise;
        gatesCorrect &= o.eor == (a.stage(1) == Stage::Fall ? kEorHigh : 0) && o.eoc == 0;
        if (checkOutputs && n.rise == int(kNineSamples / 2))
            CHECK_NEAR(o.unity1, 10 * n.rise / kNineSamples, 1e-5);
    } while (a.stage(1) == Stage::Rise && n.rise < 2 * kRate);
    if (checkOutputs) {
        CHECK_NEAR(o.unity1, 10, 0);
        CHECK_NEAR(o.eor, kEorHigh, 0);
    }
    do {
        o = a.process(kRate, p, in);
        ++n.fall;
        gatesCorrect &= o.eor == (a.stage(1) == Stage::Fall ? kEorHigh : 0);
        if (checkOutputs) gatesCorrect &= o.eoc == (a.stage(1) == Stage::Idle ? kEocHigh : 0);
        if (checkOutputs && n.fall == int(kEightSamples / 2)) {
            const double fallSamples = n.rise + n.fall - kNineSamples;
            CHECK_NEAR(o.unity1, 10 * (1 - fallSamples / kEightSamples), 1e-5);
        }
    } while (a.stage(1) == Stage::Fall && n.fall < 2 * kRate);
    CHECK_NEAR(a.stage(1) == Stage::Idle, true, 0);
    if (checkOutputs) {
        CHECK_NEAR(gatesCorrect, true, 0);
        CHECK_NEAR(o.unity1, 0, 0);
        CHECK_NEAR(o.unity4, 0, 0);
        CHECK_NEAR(o.eoc, kEocHigh, 0);
        for (int i = 0; i < 100; ++i) o = a.process(kRate, p, in);
        CHECK_NEAR(o.eoc, kEocHigh, 0);
        CHECK_NEAR(o.unity4, 0, 0);
    }
    return n;
}

static void testEnvelopeAndTimeCv() {
    Abacus::Params p;
    Abacus::Inputs in;
    p.rise1 = p.rise4 = .2f;
    p.fall1 = p.fall4 = .1f;
    auto n = measureEnvelope(p, in, true);
    CHECK_NEAR(n.rise, kNineSamples, 1);
    CHECK_NEAR(n.fall, kEightSamples, 1);

    p.fall1 = .2f;
    in.rise1Connected = in.fall1Connected = true;
    in.rise1 = 1.6f;
    in.fall1 = -1.6f;
    n = measureEnvelope(p, in);
    CHECK_NEAR(n.rise, kTenSamples, 1);
    CHECK_NEAR(n.fall, kEightSamples, 1);
    in.rise1 = -1.6f;
    in.fall1 = 1.6f;
    n = measureEnvelope(p, in);
    CHECK_NEAR(n.rise, kEightSamples, 1);
    CHECK_NEAR(n.fall, kTenSamples, 1);

    in.rise1Connected = in.fall1Connected = false;  // Ignore stale jack voltages.
    in.both1Connected = true;
    for (float volts : {-1.f, 1.f, 2.f}) {
        in.both1 = volts;
        n = measureEnvelope(p, in);
        const double expected = std::max(.0005 * kRate, kNineSamples * std::exp2(-volts));
        CHECK_NEAR(n.rise, expected, 1);
        CHECK_NEAR(n.fall, expected, 1);
    }
    in.both1 = 8;
    n = measureEnvelope(p, in);
    CHECK_NEAR(n.rise, 24, 1);
    CHECK_NEAR(n.fall, 24, 1);
}

static void testSlewAndResponse() {
    Abacus a;
    Abacus::Params p;
    Abacus::Inputs in;
    p.rise1 = .3f;
    p.fall1 = .1f;
    p.att1 = .25f;
    in.in1Connected = true;
    in.in1 = 5;
    Abacus::Outputs o;
    const int quarterRise = int(kTenSamples / 4);
    const double firstLevel = 10 * quarterRise / kTenSamples;
    for (int i = 0; i < quarterRise; ++i) o = a.process(kRate, p, in);
    CHECK_NEAR(o.unity1, firstLevel, 1e-6);
    CHECK_NEAR(o.out[0], -.5 * firstLevel, 1e-6);
    CHECK_NEAR(o.sum, -.5 * firstLevel, 1e-6);
    for (int i = 0; i < quarterRise; ++i) o = a.process(kRate, p, in);
    CHECK_NEAR(o.unity1, 2 * firstLevel, 1e-6);
    in.in1 = -5;
    const int halfFall = int(kEightSamples / 2);
    for (int i = 0; i < halfFall; ++i) o = a.process(kRate, p, in);
    CHECK_NEAR(o.unity1, 2 * firstLevel - 10 * halfFall / kEightSamples, 1e-6);
    for (int i = halfFall; i < std::ceil(kEightSamples); ++i) o = a.process(kRate, p, in);
    CHECK_NEAR(o.unity1, -5, 0);
    CHECK_NEAR(a.stage(1) == Stage::Idle, true, 0);

    float middle[3]{};
    in = {};
    in.trig1 = 5;
    in.trig1Connected = true;
    for (int i = 0; i < 3; ++i) {
        Abacus env;
        p.response1 = i / 2.f;
        const int halfRise = std::lround(.5 * kRate * env.timeSeconds(p.rise1, p.response1, false));
        for (int sample = 0; sample < halfRise; ++sample) o = env.process(kRate, p, in);
        middle[i] = o.unity1;
    }
    CHECK_NEAR(middle[0], 10 * std::pow(.5, .25), .05);
    CHECK_NEAR(middle[1], 5, .05);
    CHECK_NEAR(middle[2], .625, .05);
    CHECK_NEAR(middle[0] > middle[1] && middle[1] > middle[2], true, 0);
}

static void testCycleControlsAndEoc() {
    Abacus a;
    Abacus::Params p;
    Abacus::Inputs in;
    p.rise4 = p.fall4 = .1f;
    CHECK_NEAR(a.process(kRate, p, in).eoc, kEocHigh, 0);
    p.cycleBtn1 = true;
    a.process(kRate, p, in);
    CHECK_NEAR(a.cycling(1) && a.stage(1) == Stage::Rise, true, 0);
    for (int i = 0; i < 70; ++i) a.process(kRate, p, in);
    CHECK_NEAR(a.cycling(1), true, 0);
    p.cycleBtn1 = false;
    a.process(kRate, p, in);
    CHECK_NEAR(a.cycling(1), true, 0);
    p.cycleBtn1 = true;
    a.process(kRate, p, in);
    CHECK_NEAR(a.cycling(1), false, 0);
    for (int i = 0; i < 50; ++i) a.process(kRate, p, in);
    CHECK_NEAR(a.stage(1) == Stage::Idle, true, 0);

    in.cycle4 = 5;
    a.process(kRate, p, in);
    CHECK_NEAR(a.cycling(4), false, 0);
    in.cycle4Connected = true;
    in.cycle4 = 2.4f;
    a.process(kRate, p, in);
    CHECK_NEAR(a.cycling(4), false, 0);
    in.cycle4 = 2.5f;
    a.process(kRate, p, in);
    CHECK_NEAR(a.cycling(4) && a.stage(4) == Stage::Rise, true, 0);
    in.cycle4 = 2;
    Abacus::Outputs o;
    int elapsed = 0;
    do {
        o = a.process(kRate, p, in);
        ++elapsed;
    } while (o.eoc == 0 && elapsed < 300);
    CHECK_NEAR(elapsed, 2 * kEightSamples, 1);
    CHECK_NEAR(o.eoc, kEocHigh, 0);
    CHECK_NEAR(o.unity4, 0, 0);
    CHECK_NEAR(a.stage(4) == Stage::Rise, true, 0);
    int pulse = 0;
    while (o.eoc == kEocHigh && pulse < 100) {
        ++pulse;
        o = a.process(kRate, p, in);
    }
    CHECK_NEAR(pulse >= 48 && pulse <= 49, true, 0);
    CHECK_NEAR(o.eoc, 0, 0);
    CHECK_NEAR(a.cycling(4), true, 0);
    in.cycle4 = 1.4f;
    a.process(kRate, p, in);
    CHECK_NEAR(a.cycling(4), false, 0);
}

static void testMixBus() {
    Abacus a;
    Abacus::Params p;
    Abacus::Inputs in;
    p.att1 = p.att2 = p.att3 = p.att4 = 1;
    auto o = a.process(kRate, p, in);
    // Idle, unpatched ch 1/4 pass no internal voltage (QSG control 20/21).
    CHECK_NEAR(o.unity1, 0, 0);
    CHECK_NEAR(o.unity4, 0, 0);
    CHECK_NEAR(o.out[0], 0, 0);
    CHECK_NEAR(o.out[1], 10, 0);
    CHECK_NEAR(o.out[2], 6, 0);
    CHECK_NEAR(o.sum, 10, 0);
    CHECK_NEAR(o.inv, -10, 0);
    p.att2 = p.att3 = 0;
    o = a.process(kRate, p, in);
    CHECK_NEAR(o.out[1], -10, 0);
    CHECK_NEAR(o.out[2], -5, 0);
    CHECK_NEAR(o.sum, -10, 0);
    CHECK_NEAR(o.inv, 10, 0);
    CHECK_NEAR(o.orOut, 0, 0);
    p.att2 = p.att3 = .5f;
    o = a.process(kRate, p, in);
    CHECK_NEAR(o.out[1], 0, 0);
    CHECK_NEAR(o.out[2], 0, 0);
    CHECK_NEAR(o.sum, 0, 0);

    p.att2 = .75f;
    p.att3 = .25f;
    in.in2Connected = in.in3Connected = true;
    in.in2 = 8;
    in.in3 = 6;
    o = a.process(kRate, p, in);
    CHECK_NEAR(o.out[1], 4, 0);
    CHECK_NEAR(o.out[2], -3, 0);
    CHECK_NEAR(o.sum, 1, 0);
    CHECK_NEAR(o.inv, -1, 0);
    CHECK_NEAR(o.orOut, 4, 0);
    in.outConnected[1] = true;
    o = a.process(kRate, p, in);
    CHECK_NEAR(o.out[1], 4, 0);
    CHECK_NEAR(o.sum, -3, 0);
    CHECK_NEAR(o.inv, 3, 0);
    CHECK_NEAR(o.orOut, 0, 0);
    p.patchedOutLeavesBus = false;
    o = a.process(kRate, p, in);
    CHECK_NEAR(o.sum, 1, 0);
    CHECK_NEAR(o.inv, -1, 0);
    CHECK_NEAR(o.orOut, 4, 0);

    p.att2 = p.att3 = .5f;
    in.trig1 = 5;
    in.trig1Connected = true;
    o = a.process(kRate, p, in);
    CHECK_NEAR(o.sum, o.unity1, 0);
    CHECK_NEAR(o.orOut, o.unity1, 0);
    CHECK_NEAR(o.unity1 > 0, true, 0);
}

static void testCycleTimingAndSampleRates() {
    Abacus::Params p;
    p.cycleBtn1 = true;
    p.fall1 = k280::knobFromClock(9);
    const double periodsMs[] = {3.2, 5.8, 16, 45};  // bench 2026-11-22, test 15a
    for (int i = 0; i < 4; ++i) {
        Abacus a;
        p.rise1 = k280::knobFromClock(9 + i);
        int firstEdge = -1, lastEdge = -1, edges = 0;
        bool previous = false;
        for (int sample = 0; sample < kRate / 2; ++sample) {
            const bool high = a.process(kRate, p, {}).eor == kEorHigh;
            if (high && !previous) {
                if (edges++ == 0) firstEdge = sample;
                lastEdge = sample;
            }
            previous = high;
        }
        CHECK_NEAR(edges > 1, true, 0);
        const double periodMs = (lastEdge - firstEdge) * 1000. / (kRate * (edges - 1));
        CHECK_NEAR(periodMs, periodsMs[i], .15 * periodsMs[i]);
    }

    for (float rate : {44100.f, 48000.f, 96000.f, 192000.f}) {
        Abacus fast;
        p = {};
        p.cycleBtn1 = p.cycleBtn4 = true;
        p.response4 = 1;
        p.att1 = p.att2 = p.att3 = p.att4 = 1;
        bool finite = true, bounded = true;
        int count = 0;
        bool prev = false;
        for (int i = 0; i < rate / 10; ++i) {
            const auto o = fast.process(rate, p, {});
            for (float x : o.out) finite &= std::isfinite(x);
            for (float x : {o.unity1, o.unity4, o.eor, o.eoc, o.orOut, o.sum, o.inv})
                finite &= std::isfinite(x);
            bounded &= o.unity1 >= 0 && o.unity1 <= 10 && o.unity4 >= 0 && o.unity4 <= 10;
            const bool high = o.eor == kEorHigh;
            count += high && !prev;
            prev = high;
        }
        CHECK_NEAR(finite, true, 0);
        CHECK_NEAR(bounded, true, 0);
        CHECK_NEAR(count >= 100, true, 0);
    }
}

int main() {
    testTimeTable();
    testTriggerAndRetrigger();
    testTriggerRamps();
    testEnvelopeAndTimeCv();
    testSlewAndResponse();
    testCycleControlsAndEoc();
    testMixBus();
    testCycleTimingAndSampleRates();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
