// Knob positions. The book gives knobs as clock faces: 7 o'clock = fully counter-clockwise,
// noon = centre, 5 o'clock = fully clockwise (300 degrees of travel). Every knob in the engine
// takes a normalised position 0..1 over that travel, so "2 o'clock" means the same angle in the
// book, the emulator and on the rack. VCV's default knob travel (±150 degrees) matches.
#pragma once
#include <cmath>

namespace k280 {

// 7.0 .. 17.0 on a 24-hour-style dial (7 = 7 o'clock, 12 = noon, 14 = 2 o'clock, 17 = 5 o'clock).
inline float knobFromClock(float hour24) {
    if (hour24 < 7.f) hour24 += 12.f;  // accept 1..5 for afternoon positions
    float k = (hour24 - 7.f) / 10.f;
    return k < 0.f ? 0.f : (k > 1.f ? 1.f : k);
}

inline float clockFromKnob(float knob) { return 7.f + 10.f * knob; }

}  // namespace k280
