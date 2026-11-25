// Golden tests for the Quantimator model in sim/docs/m5-specs.md.
#include <cstring>
#include <initializer_list>
#include <limits>

#include "modules/Quantimator.hpp"
#include "modules/Seq960.hpp"
#include "test_main.hpp"

using k280::Quantimator;
constexpr float kRate = 48000.f;

static void checkNotes(const Quantimator::Outputs& out, int a, int b, int c) {
    const int expected[] = {a, b, c};
    for (int i = 0; i < 3; ++i) CHECK_NEAR(out.out[i], expected[i] / 12.0, 1e-6);
}

static void testChromaticAndHysteresis() {
    Quantimator::Params p;
    p.scaleMode = true;
    p.transpose = 0.f;
    Quantimator::Inputs in;
    in.inConnected = true;
    Quantimator q;
    for (int note = 0; note < 60; ++note) {
        in.inV = (note + 0.25f) / 12.f;
        checkNotes(q.process(kRate, p, in), note, note, 0);
        in.inV = (note + 0.75f) / 12.f;
        checkNotes(q.process(kRate, p, in), note + 1, note + 1, 0);

        Quantimator fresh;
        in.inV = (note + 0.5f) / 12.f;
        checkNotes(fresh.process(kRate, p, in), note, note, 0);
    }

    const float boundary = 12.5f / 12.f;
    for (int previous : {12, 13}) {
        in.inV = previous / 12.f;
        q.process(kRate, p, in);
        for (int sample = 0; sample < 32; ++sample) {
            in.inV = boundary + (sample % 2 ? 0.002f : -0.002f);
            checkNotes(q.process(kRate, p, in), previous, previous, 0);
        }
        in.inV = boundary + (previous == 12 ? 0.006f : -0.006f);
        checkNotes(q.process(kRate, p, in), 25 - previous, 25 - previous, 0);
    }
}

static void testScales() {
    Quantimator q;
    Quantimator::Params p;
    p.scaleMode = true;
    p.transpose = 0.f;
    Quantimator::Inputs in;
    in.inConnected = true;
    const int indices[] = {1, 7, 8};
    const int golden[][12] = {
        {0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9, 11},
        {0, 0, 2, 2, 4, 4, 7, 7, 7, 9, 9, 12},
        {0, 0, 3, 3, 3, 5, 5, 7, 7, 10, 10, 10},
    };
    for (int bank = 0; bank < 3; ++bank) {
        p.chordScale = indices[bank] / 32.f;
        for (int semitone = 0; semitone < 60; ++semitone) {
            in.inV = semitone / 12.f;
            const int note = semitone / 12 * 12 + golden[bank][semitone % 12];
            checkNotes(q.process(kRate, p, in), note, note, 0);
        }
    }
    // The intermediate semitone is 3. Ionian then breaks the 2/4 tie downward.
    p.chordScale = 1 / 32.f;
    in.inV = 3.25f / 12.f;
    checkNotes(q.process(kRate, p, in), 2, 2, 0);
}

static void testTypeAndTranspose() {
    Quantimator q;
    Quantimator::Params p;
    for (int index = 0; index < 32; ++index) {
        p.chordScale = index / 32.f;
        CHECK_NEAR(q.typeIndex(p, 0.f), index, 0);
        CHECK_NEAR(q.typeIndex(p, 0.07f), index, 0);
    }
    p.chordScale = 0.25f;
    CHECK_NEAR(q.typeIndex(p, 1.25f), 16, 0);
    CHECK_NEAR(q.typeIndex(p, -1.25f), 0, 0);
    CHECK_NEAR(q.typeIndex(p, -5.f), 0, 0);
    CHECK_NEAR(q.typeIndex(p, 5.f), 31, 0);
    p.csAtt = 0.5f;
    CHECK_NEAR(q.typeIndex(p, 1.25f), 12, 0);
    p.csAtt = 0.f;
    CHECK_NEAR(q.typeIndex(p, 5.f), 8, 0);
    CHECK_NEAR(std::strcmp(Quantimator::scaleName(19), "zirafkend"), 0, 0);
    CHECK_NEAR(std::strcmp(Quantimator::scaleName(31), "prism"), 0, 0);
    CHECK_NEAR(std::strcmp(Quantimator::chordName(26), "minor inv1"), 0, 0);
    CHECK_NEAR(std::strcmp(Quantimator::chordName(31), "5ths"), 0, 0);

    p = {};
    p.scaleMode = true;
    p.transpose = 0.f;
    Quantimator::Inputs in;
    in.inV = 14.f / 12.f;
    checkNotes(q.process(kRate, p, in), 0, 0, 0);
    in.inConnected = true;
    in.trigV = in.holdV = 5.f;  // Unpatched gates are ignored.
    in.transV = 0.13f;
    in.cscvV = 7.f * 5.f / 32.f;
    checkNotes(q.process(kRate, p, in), 14, 14, 0);
    p.transpose = 0.5f;
    checkNotes(q.process(kRate, p, in), 20, 20, 0);
    p.transpose = 1.f;
    checkNotes(q.process(kRate, p, in), 25, 25, 0);
    p.transpose = 0.49f;
    checkNotes(q.process(kRate, p, in), 19, 19, 0);
    p.transpose = 0.52f;
    in.transConnected = true;
    checkNotes(q.process(kRate, p, in), 22, 22, 0);
    in.transV = 0.12f;
    checkNotes(q.process(kRate, p, in), 21, 21, 0);
    in.transV = -1.f;
    checkNotes(q.process(kRate, p, in), 20, 20, 0);

    in.inV = 30.f / 12.f;
    checkNotes(q.process(kRate, p, in), 36, 36, 0);
    in.cscvConnected = true;
    in.transV = 1.f / 12.f;
    checkNotes(q.process(kRate, p, in), 38, 38, 0);  // C pentatonic G, then transpose.
    in.transV = 5.f;
    checkNotes(q.process(kRate, p, in), 60, 60, 0);

    q.cal.outputMaxV = 10.f;
    p.transpose = 0.5f;
    in.inV = 0.1f;
    in.cscvConnected = false;
    in.transV = 6.f;
    checkNotes(q.process(kRate, p, in), 67, 67, 0);
}

static void testScaleTriggersAndHold() {
    Quantimator q;
    Quantimator::Params p;
    p.scaleMode = true;
    p.transpose = 0.f;
    Quantimator::Inputs in;
    in.inConnected = in.trigConnected = true;
    const auto step = [&](float noteV, float trig, int a, int b, int c) {
        in.inV = noteV;
        in.trigV = trig;
        checkNotes(q.process(kRate, p, in), a, b, c);
    };
    step(1.f, 1.49f, 0, 0, 0);
    step(1.f, 1.5f, 12, 0, 0);
    step(2.f, 5.f, 12, 0, 0);
    step(2.f, 1.f, 12, 0, 0);
    step(2.f, 1.5f, 12, 0, 0);
    step(2.f, 0.99f, 12, 0, 0);
    step(2.f, 1.5f, 24, 12, 0);
    step(3.f, 0.f, 24, 12, 0);
    step(3.f, 1.5f, 36, 24, 12);

    in.holdConnected = true;
    in.holdV = 1.5f;
    step(4.f, 5.f, 36, 24, 12);
    step(4.f, 0.f, 36, 24, 12);
    step(4.f, 1.5f, 12, 36, 24);
    in.holdV = 1.f;
    step(4.f, 0.f, 12, 36, 24);
    step(4.f, 1.5f, 24, 12, 36);
    step(4.f, 0.f, 24, 12, 36);
    step(4.f, 1.5f, 36, 24, 12);
    in.holdV = 0.99f;
    step(4.f, 0.f, 36, 24, 12);
    step(4.f, 1.5f, 48, 36, 24);

    in.trigConnected = false;
    in.holdV = 1.5f;
    step(1.f, 5.f, 48, 36, 24);
    step(2.f, 0.f, 48, 36, 24);
    in.holdConnected = false;
    step(2.f, 5.f, 24, 24, 0);
    step(3.f, 5.f, 36, 36, 0);
}

static void testChords() {
    Quantimator q;
    Quantimator::Params p;
    p.transpose = 0.f;
    Quantimator::Inputs in;
    in.inConnected = true;
    in.inV = 1.24f;  // D sharp, rounded chromatically to semitone 15.
    const int tones[][3] = {
        {0, 4, 7}, {4, 7, 12}, {7, 12, 16}, {0, 4, 8}, {4, 8, 12}, {8, 12, 16},
        {0, 4, 9}, {4, 9, 12}, {9, 12, 16}, {0, 4, 10}, {4, 10, 12}, {10, 12, 16},
        {0, 5, 7}, {5, 7, 12}, {0, 5, 8}, {5, 8, 12}, {0, 5, 9}, {5, 9, 12},
        {9, 12, 17}, {0, 3, 9}, {3, 9, 12}, {9, 12, 15}, {0, 3, 8}, {3, 8, 12},
        {8, 12, 15}, {0, 3, 7}, {3, 7, 12}, {7, 12, 15}, {0, 3, 6}, {3, 6, 12},
        {6, 12, 15}, {0, 7, 14},
    };
    for (int index = 0; index < 32; ++index) {
        p.chordScale = index / 32.f;
        checkNotes(q.process(kRate, p, in), 15 + tones[index][0], 15 + tones[index][1], 15 + tones[index][2]);
    }

    p.chordScale = 0.f;
    in.inV = 59.f / 12.f;
    checkNotes(q.process(kRate, p, in), 59, 60, 60);
    in.inV = 1.f;
    in.trigConnected = true;
    in.trigV = 1.5f;
    checkNotes(q.process(kRate, p, in), 12, 16, 19);
    in.holdConnected = true;
    in.holdV = 5.f;
    in.inV = 2.f;
    p.chordScale = 25 / 32.f;
    p.transpose = 1.f;
    checkNotes(q.process(kRate, p, in), 12, 16, 19);
    // Quantimator_Manual.txt, p. 1: chord HOLD refreshes controls at each trigger.
    in.trigV = 0.f;
    checkNotes(q.process(kRate, p, in), 12, 16, 19);
    in.trigV = 1.5f;
    checkNotes(q.process(kRate, p, in), 42, 35, 38);

    in.transConnected = in.cscvConnected = true;
    in.transV = 1.f / 12.f;
    in.cscvV = 5.f / 32.f;  // Minor -> minor first inversion.
    checkNotes(q.process(kRate, p, in), 42, 35, 38);
    in.trigV = 0.f;
    checkNotes(q.process(kRate, p, in), 42, 35, 38);
    in.trigV = 1.5f;
    checkNotes(q.process(kRate, p, in), 43, 48, 39);
    in.trigV = 0.f;
    q.process(kRate, p, in);
    in.trigV = 1.5f;
    checkNotes(q.process(kRate, p, in), 39, 43, 48);

    in.holdV = 0.f;
    in.transConnected = in.cscvConnected = false;
    checkNotes(q.process(kRate, p, in), 39, 43, 48);
    in.trigV = 0.f;
    q.process(kRate, p, in);
    in.trigV = 1.5f;
    checkNotes(q.process(kRate, p, in), 35, 38, 42);
}

static void testBookPatch18() {
    k280::Seq960 seq;
    k280::Seq960::Params seqParams;
    k280::Seq960::Inputs clock;
    Quantimator q;
    Quantimator::Params p;
    p.scaleMode = true;
    p.chordScale = 7 / 32.f;
    Quantimator::Inputs in;
    in.inConnected = in.trigConnected = in.holdConnected = true;
    const float row[] = {0.14f, 0.29f, 0.36f, 0.43f, 0.58f, 0.67f, 0.72f, 0.86f};
    for (int stage = 0; stage < 8; ++stage) seqParams.knobs[0][stage] = row[stage];

    // Independent clocks advance the 960 every seven samples and capture every five.
    // Start HOLD after all three taps contain quantized row pitches.
    const bool pentatonic[] = {true, false, true, false, true, false,
                               false, true, false, true, false, false};
    Quantimator::Outputs previous;
    for (int sample = 0; sample < 160; ++sample) {
        clock.shift = sample > 0 && sample % 7 == 0;
        in.inV = seq.process(kRate, seqParams, clock).row[0];
        in.trigV = sample % 5 == 0 ? 5.f : 0.f;
        in.holdV = sample >= 80 ? 6.f : 0.f;
        const auto out = q.process(kRate, p, in);
        for (int tap = 0; tap < 3; ++tap) {
            const float volts = out.out[tap];
            if (sample < tap * 5) {
                CHECK_NEAR(volts, 0.f, 0);
                continue;
            }
            const bool inRange = volts >= 0.f && volts <= 5.f;
            CHECK_NEAR(inRange, true, 0);
            if (!inRange) continue;
            const int note = static_cast<int>(std::round(volts * 12.f));
            CHECK_NEAR(volts, note / 12.0, 1e-6);
            CHECK_NEAR(pentatonic[(note + 6) % 12], true, 0);
        }
        if (in.holdV && in.trigV) {
            for (int tap = 0; tap < 3; ++tap)
                CHECK_NEAR(out.out[tap], previous.out[(tap + 2) % 3], 0);
        }
        previous = out;
    }
}

static void testFinite() {
    Quantimator q;
    Quantimator::Params p;
    Quantimator::Inputs in;
    in.inConnected = in.transConnected = in.cscvConnected = true;
    in.inV = std::numeric_limits<float>::infinity();
    in.transV = -std::numeric_limits<float>::infinity();
    in.cscvV = std::numeric_limits<float>::quiet_NaN();
    p.transpose = in.cscvV;
    for (bool scaleMode : {false, true}) {
        p.scaleMode = scaleMode;
        for (int index = 0; index < 32; ++index) {
            p.chordScale = index / 32.f;
            for (float volts : q.process(kRate, p, in).out)
                CHECK_NEAR(std::isfinite(volts), true, 0);
        }
    }
}

int main() {
    testChromaticAndHysteresis();
    testScales();
    testTypeAndTranspose();
    testScaleTriggersAndHold();
    testChords();
    testBookPatch18();
    testFinite();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
