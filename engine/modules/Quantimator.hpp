// Toppobrillo Quantimator. Model and source: sim/docs/m5-specs.md.
#pragma once

#include <algorithm>
#include <cmath>

#include "Confidence.hpp"

namespace k280 {

struct QuantimatorCalibration {
    float trigOnV = 1.5f;
    float holdOnV = 1.5f;
    float gateHysteresisV = 0.5f;
    float pitchHysteresisV = 0.010f;  // ASSUMPTION: total width centered on each midpoint.
    static constexpr int defaultTransposeKnobSpan = 11;  // bench 2026-11-22, test 17
    int transposeKnobSpan = defaultTransposeKnobSpan;
    float outputMaxV = 5.f;          // ASSUMPTION: clamp to the guide's range.
    float inputMaxV = 5.f;
    float transposeMaxV = 5.f;
    float typeCvSpanV = 5.f;
    float typePositionMax = 0.9999f;

    static constexpr int octave = 12;
    static constexpr int typeCount = 32;

    struct Scale {
        const char* name;
        int degrees[octave];  // Pitch classes in semitones. Unused entries repeat the tonic.
    };

    inline static constexpr Scale scales[typeCount] = {
        {"chromatic", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},  // All semitones.
        {"ionian", {0, 2, 4, 5, 7, 9, 11}},                    // Major diatonic.
        {"dorian", {0, 2, 3, 5, 7, 9, 10}},                    // Minor with a raised sixth.
        {"lydian", {0, 2, 4, 6, 7, 9, 11}},                    // Major with a raised fourth.
        {"phrygian", {0, 1, 3, 5, 7, 8, 10}},                  // Minor with a lowered second.
        {"aeolian", {0, 2, 3, 5, 7, 8, 10}},                   // Natural minor.
        {"locrian", {0, 1, 3, 5, 6, 8, 10}},                   // Lowered second and fifth.
        {"major pentatonic", {0, 2, 4, 7, 9}},                 // Major without fourth and seventh.
        {"minor pentatonic", {0, 3, 5, 7, 10}},                // Minor without second and sixth.
        {"mixolydian", {0, 2, 4, 5, 7, 9, 10}},                // Major with a lowered seventh.
        {"blues", {0, 3, 5, 6, 7, 10}},                        // Minor pentatonic plus the blue fifth.
        {"diminished", {0, 2, 3, 5, 6, 8, 9, 11}},             // Whole-half octatonic.
        {"oriental", {0, 1, 4, 5, 6, 9, 10}},                  // Oriental heptatonic.
        {"whole tone", {0, 2, 4, 6, 8, 10}},                   // Regular whole steps.
        {"sixtone symmetrical", {0, 1, 4, 5, 8, 9}},           // ASSUMPTION: alternating half step and minor third.
        {"spanish", {0, 1, 3, 4, 5, 7, 8, 10}},                // Eight-tone Spanish (both thirds).
        {"algerian", {0, 2, 3, 5, 6, 7, 8, 11}},               // Algerian with both fifths.
        {"bali pelog", {0, 1, 3, 7, 8}},                       // Five-tone equal-tempered pelog approximation.
        {"arabian", {0, 2, 4, 5, 6, 8, 10}},                   // Arabian major, also called major Locrian.
        {"zirafkend", {0, 2, 3, 5, 7, 8, 9, 11}},              // ASSUMPTION: harmonic minor with both sixths.
        {"spanish gypsy", {0, 1, 4, 5, 7, 8, 10}},              // Phrygian dominant.
        {"hungarian", {0, 2, 3, 6, 7, 8, 11}},                  // Hungarian minor.
        {"egyptian", {0, 2, 5, 7, 10}},                         // Suspended pentatonic.
        {"persian", {0, 1, 4, 5, 6, 8, 11}},                    // Persian heptatonic.
        {"enigmatic", {0, 1, 4, 6, 8, 10, 11}},                 // Ascending enigmatic.
        {"tritone", {0, 1, 4, 6, 7, 10}},                       // Two 0-1-4 cells a tritone apart.
        {"surf", {0, 1, 4, 5, 7, 8, 11}},                       // ASSUMPTION: double harmonic major for a surf sound.
        {"major", {0, 4, 7}},                                  // ASSUMPTION: major triad pitch classes.
        {"minor", {0, 3, 7}},                                  // ASSUMPTION: minor triad pitch classes.
        {"inversions major", {0, 3, 5, 8, 9}},                 // ASSUMPTION: first and second major inversions relative to their basses.
        {"inversions minor", {0, 4, 5, 8, 9}},                 // ASSUMPTION: first and second minor inversions relative to their basses.
        {"prism", {0, 1, 2, 4, 5, 6, 8, 9, 10}},              // ASSUMPTION: three chromatic cells a major third apart.
    };

    struct Chord {
        const char* name;
        int tones[3];  // Semitone offsets from the quantized root in output order.
    };

    inline static constexpr Chord chords[typeCount] = {
        {"major", {0, 4, 7}},            // Root position.
        {"major inv1", {4, 7, 12}},      // Third in the bass.
        {"major inv2", {7, 12, 16}},     // Fifth in the bass.
        {"augmented", {0, 4, 8}},        // Major third and raised fifth.
        {"aug inv1", {4, 8, 12}},        // Third in the bass.
        {"aug inv2", {8, 12, 16}},       // Raised fifth in the bass.
        {"6th", {0, 4, 9}},              // ASSUMPTION: omit fifth.
        {"6th inv1", {4, 9, 12}},        // ASSUMPTION: invert the three retained tones.
        {"6th inv2", {9, 12, 16}},       // ASSUMPTION: invert the three retained tones.
        {"7th", {0, 4, 10}},             // ASSUMPTION: omit fifth from dominant seventh.
        {"7th inv1", {4, 10, 12}},       // ASSUMPTION: invert the three retained tones.
        {"7th inv2", {10, 12, 16}},      // ASSUMPTION: invert the three retained tones.
        {"sus4", {0, 5, 7}},             // Fourth replaces third.
        {"sus4 inv", {5, 7, 12}},        // ASSUMPTION: unlabeled inversion is first inversion.
        {"sus4 aug5", {0, 5, 8}},        // Suspended fourth and raised fifth.
        {"sus4 aug5 inv", {5, 8, 12}},   // ASSUMPTION: unlabeled inversion is first inversion.
        {"sus4 6", {0, 5, 9}},           // ASSUMPTION: retain root, fourth and sixth.
        {"sus4 6 inv1", {5, 9, 12}},     // ASSUMPTION: invert the three retained tones.
        {"sus4 6 inv2", {9, 12, 17}},    // ASSUMPTION: invert the three retained tones.
        {"m6th", {0, 3, 9}},             // ASSUMPTION: omit fifth.
        {"m6th inv1", {3, 9, 12}},       // ASSUMPTION: invert the three retained tones.
        {"m6th inv2", {9, 12, 15}},      // ASSUMPTION: invert the three retained tones.
        {"mAug5", {0, 3, 8}},            // Minor third and raised fifth.
        {"mAug5 inv1", {3, 8, 12}},      // Third in the bass.
        {"mAug5 inv2", {8, 12, 15}},     // Raised fifth in the bass.
        {"minor", {0, 3, 7}},            // Root position.
        {"minor inv1", {3, 7, 12}},      // Third in the bass.
        {"minor inv2", {7, 12, 15}},     // Fifth in the bass.
        {"diminished", {0, 3, 6}},       // Minor third and lowered fifth.
        {"diminished inv1", {3, 6, 12}}, // Third in the bass.
        {"diminished inv2", {6, 12, 15}}, // Lowered fifth in the bass.
        {"5ths", {0, 7, 14}},            // ASSUMPTION: two stacked perfect fifths.
    };
};

class Quantimator {
public:
    static constexpr Confidence kConfidence = Confidence::Manual;

    struct Params {
        bool scaleMode = false;
        float chordScale = 0.f;
        float csAtt = 1.f;
        float transpose = 0.5f;
    };

    struct Inputs {
        float inV = 0.f, trigV = 0.f, holdV = 0.f, transV = 0.f, cscvV = 0.f;
        bool inConnected = false, trigConnected = false, holdConnected = false;
        bool transConnected = false, cscvConnected = false;
    };

    struct Outputs {
        float out[3]{};
    };

    QuantimatorCalibration cal;

    int typeIndex(const Params& p, float cscv) const {
        const float position = finiteOrZero(p.chordScale) +
            finiteOrZero(cscv) * std::clamp(finiteOrZero(p.csAtt), 0.f, 1.f) / cal.typeCvSpanV;
        return static_cast<int>(C::typeCount * std::clamp(position, 0.f, cal.typePositionMax));
    }

    static const char* scaleName(int index) {
        return C::scales[std::clamp(index, 0, C::typeCount - 1)].name;
    }

    static const char* chordName(int index) {
        return C::chords[std::clamp(index, 0, C::typeCount - 1)].name;
    }

    Outputs process(float /* sampleRate */, const Params& p, const Inputs& in) {
        const bool trigger = trigJack_.rise(in.trigV, in.trigConnected, cal.trigOnV, cal.gateHysteresisV);
        holdJack_.rise(in.holdV, in.holdConnected, cal.holdOnV, cal.gateHysteresisV);

        const bool chordHold = holdJack_.high && in.trigConnected && !p.scaleMode;
        if (!chordHold) chordRotation_ = 0;
        if (in.trigConnected && !trigger) return outputs_;
        if (holdJack_.high && !chordHold) {
            if (trigger) rotateOutputs();
            return outputs_;
        }

        const int index = typeIndex(p, in.cscvConnected ? in.cscvV : 0.f);
        const int semitone = quantizeSemitone(in.inConnected ? in.inV : 0.f);
        const int transpose = transposeSemitones(p, in);
        if (p.scaleMode) {
            const float note = outputVoltage(nearestDegree(semitone, C::scales[index]) + transpose);
            outputs_ = in.trigConnected ? Outputs{{note, outputs_.out[0], outputs_.out[1]}}
                                        : Outputs{{note, note, 0.f}};  // bench 2026-11-22, test 17
        } else {
            // ASSUMPTION: chord roots are chromatic.
            // Quantimator_Manual.txt, p. 1: sample all controls on each chord HOLD trigger.
            if (chordHold) chordRotation_ = (chordRotation_ + 1) % 3;
            for (int i = 0; i < 3; ++i)
                outputs_.out[(i + chordRotation_) % 3] =
                    outputVoltage(semitone + transpose + C::chords[index].tones[i]);
        }
        return outputs_;
    }

private:
    using C = QuantimatorCalibration;

    struct Comparator {
        bool high = false;

        bool rise(float volts, bool connected, float threshold, float hysteresis) {
            const bool wasHigh = high;
            high = connected && volts >= (wasHigh ? threshold - hysteresis : threshold);
            return high && !wasHigh;
        }
    };

    static float finiteOrZero(float volts) {
        return std::isfinite(volts) ? volts : 0.f;
    }

    int quantizeSemitone(float volts) {
        const float pitch = std::clamp(finiteOrZero(volts), 0.f, cal.inputMaxV);
        const float hysteresis = cal.pitchHysteresisV * 0.5f;
        const float upperBoundary = (lastSemitone_ + 0.5f) / C::octave + hysteresis;
        const float lowerBoundary = (lastSemitone_ - 0.5f) / C::octave - hysteresis;
        if (!hasPitch_ || pitch > upperBoundary || pitch <= lowerBoundary) {
            const int lower = static_cast<int>(pitch * C::octave);
            lastSemitone_ = lower + (pitch > (lower + 0.5f) / C::octave);
            hasPitch_ = true;
        }
        return lastSemitone_;
    }

    static int nearestDegree(int semitone, const C::Scale& scale) {
        const int octaveBase = semitone / C::octave * C::octave;
        const int pitchClass = semitone - octaveBase;
        int nearest = C::octave;  // The tonic of the next octave is also a candidate.
        for (int degree : scale.degrees) {
            const int distance = std::abs(pitchClass - degree);
            const int bestDistance = std::abs(pitchClass - nearest);
            if (distance < bestDistance || (distance == bestDistance && degree < nearest))
                nearest = degree;
        }
        return octaveBase + nearest;
    }

    int transposeSemitones(const Params& p, const Inputs& in) const {
        const float knob = std::isfinite(p.transpose) ? std::clamp(p.transpose, 0.f, 1.f) : 0.5f;
        const float cv = in.transConnected ? std::clamp(finiteOrZero(in.transV), 0.f, cal.transposeMaxV) : 0.f;
        return static_cast<int>(std::round(knob * cal.transposeKnobSpan)) +
               static_cast<int>(std::round(cv * C::octave));
    }

    float outputVoltage(int semitone) const {
        return std::clamp(static_cast<float>(semitone) / C::octave, 0.f, cal.outputMaxV);
    }

    void rotateOutputs() {
        outputs_ = Outputs{{outputs_.out[2], outputs_.out[0], outputs_.out[1]}};
    }

    Comparator trigJack_, holdJack_;
    Outputs outputs_;
    int chordRotation_ = 0;
    int lastSemitone_ = 0;
    bool hasPitch_ = false;
};

}  // namespace k280
