#include "test_main.hpp"
#include "modules/Vco921Circuit.hpp"

#include <chrono>
#include <type_traits>

using k280::Vco921Circuit;

int main() {
    static_assert(Vco921Circuit::kConfidence == k280::Confidence::Schematic);
    static_assert(std::is_same_v<Vco921Circuit::Params, k280::Vco921::Params>);
    Vco921Circuit v;
    Vco921Circuit::Params p;
    Vco921Circuit::Inputs in;
    p.range = Vco921Circuit::R2;
    p.semitoneScale = true;
    for (auto target : {160., 640., 2560.}) {
        in.fcv[0] = float(std::log2(target / 640.));
        CHECK_NEAR(v.frequencyHz(p, in.fcv[0]), target, 0.001);
        Vco921Circuit core;
        int cycles = 0;
        float last = 0;
        for (int i = 0; i < 48000; ++i) {
            const auto out = core.process(48000, p, in);
            cycles += last < 0 && out.saw >= 0;
            last = out.saw;
        }
        CHECK_NEAR(cycles, target, 1);
    }
    for (int range = v.R16; range <= v.R1; ++range) {
        p.range = range - 1;
        const double low = v.frequencyHz(p, 0);
        ++p.range;
        CHECK_NEAR(1200 * std::log2(v.frequencyHz(p, 0) / low), 1200, 1);
    }
    p.range = v.R8;
    in.fcv[0] = 0;
    CHECK_NEAR(v.frequencyHz(p, 0), 160, 0.001);
    CHECK_NEAR(v.frequencyHz(p, 8) < 160 * 256, true, 0);
    int falls = 0, high = 0;
    float last = 0;
    for (int i = 0; i < 48000; ++i) {
        const auto out = v.process(48000, p, in);
        falls += out.saw < last;
        high += out.rect > 0;
        last = out.saw;
    }
    CHECK_NEAR(falls / 48000., 1, 0.02);
    CHECK_NEAR(high / 48000., 0.5 * (1.0 - 160e-5), 0.004);
    CHECK_NEAR(Vco921Circuit::sine(0.5), 1, 1e-6);

    in.vtrigConnected = true;
    in.vtrig = 5.f;
    v.process(48000, p, in);
    CHECK_NEAR(v.clamped(), true, 0);
    CHECK_NEAR(v.phase(), 0.5, 1e-8);
    for (int i = 0; i < 51; ++i) v.process(48000, p, in);
    CHECK_NEAR(v.clamped(), false, 0);  // The held gate can't retrigger the one-shot.
    CHECK_NEAR(v.phase() > 0.5, true, 0);

    const auto start = std::chrono::steady_clock::now();
    int invalid = 0;
    for (int i = 0; i < 480000; ++i) {
        in.fcv[0] = float(i % 1800) / 100.f - 9.f;
        in.pw[0] = (i % 100) / 17.f - 3.f;
        in.vtrig = i % 371 < 12 ? 5.f : 0.f;
        p.subAudio = i % 4000 > 2000;
        p.auxLevel = 1.f;
        p.auxWave = i % v.NUM_AUX;
        const auto out = v.process(48000, p, in);
        for (float x : {out.sine, out.tri, out.saw, out.rect, out.aux, out.auxNeg})
            invalid += !std::isfinite(x) || std::fpclassify(x) == FP_SUBNORMAL;
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    CHECK_NEAR(invalid, 0, 0);
    CHECK_NEAR(seconds < 10.0, true, 0);
    std::printf("10 seconds of audio processed in %.3f seconds\n", seconds);
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
