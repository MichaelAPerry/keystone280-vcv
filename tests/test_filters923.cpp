// Golden tests for the 923 filter response and noise levels in m4-specs.md.
#include <array>
#include <complex>
#include <vector>

#include "modules/Filters923.hpp"
#include "test_main.hpp"

using k280::Filters923;

static constexpr float kRate = 48000.f;
static constexpr double kPi = 3.14159265358979323846;

static void testTaper() {
    Filters923 module;
    CHECK_NEAR(module.cutoffHz(-.1f), 6, 1e-6);
    CHECK_NEAR(module.cutoffHz(1.1f), 6000, 1e-3);
    CHECK_NEAR(module.cutoffHz(-.1f, true), 4.7, 1e-6);
    CHECK_NEAR(module.cutoffHz(1.1f, true), 4700, 1e-3);
    for (int i = 0; i <= 20; ++i) {
        const float knob = i / 20.f;
        const double hz = 10 * std::pow(1000., knob);
        CHECK_NEAR(module.cutoffHz(knob), .6 * hz, hz * 1e-6);
        CHECK_NEAR(module.cutoffHz(knob, true), .47 * hz, hz * 1e-6);
    }

    module.cal.cutoffTable[5] = {.52f, 400.f};
    CHECK_NEAR(module.cutoffHz(.52f), 240, 1e-4);
    CHECK_NEAR(module.cutoffHz(.46f), .6 * std::sqrt(158.489319 * 400), 1e-4);
}

struct Levels {
    double lpDb;
    double hpDb;
};

static Levels measureTone(float rate, Filters923::Params knobs, double hz) {
    Filters923 module;
    const double phaseStep = 2 * kPi * hz / rate;
    const int warmup = int(std::ceil(32 * rate / hz));
    const int length = int(std::ceil(128 * rate / hz));
    double sourcePower = 0, lpPower = 0, hpPower = 0;
    for (int i = 0; i < warmup + length; ++i) {
        const float input = float(std::sin(i * phaseStep));
        const auto out = module.process(rate, knobs, {input, input});
        if (i < warmup) continue;
        sourcePower += double(input) * input;
        lpPower += double(out.lpOut) * out.lpOut;
        hpPower += double(out.hpOut) * out.hpOut;
    }
    return {10 * std::log10(lpPower / sourcePower), 10 * std::log10(hpPower / sourcePower)};
}

static void testFilters() {
    CHECK_NEAR(measureTone(kRate, {.5f, .5f}, 190).lpDb, -3.0103, .25);
    CHECK_NEAR(measureTone(kRate, {.5f, .5f}, 150).hpDb, -3.0103, .25);

    // Keep slope probes in the stopbands and away from Nyquist warping.
    for (float knob : {.25f, .5f, .7f}) {
        const double lpHz = 6 * std::pow(1000., knob);
        const double hpHz = 4.7 * std::pow(1000., knob);
        const Filters923::Params p{knob, knob};
        CHECK_NEAR(measureTone(kRate, p, lpHz).lpDb, -3.0103, .5);
        CHECK_NEAR(measureTone(kRate, p, hpHz).hpDb, -3.0103, .5);
        CHECK_NEAR(measureTone(kRate, p, lpHz * 8).lpDb -
                   measureTone(kRate, p, lpHz * 4).lpDb, -6, 1);
        CHECK_NEAR(measureTone(kRate, p, hpHz / 8).hpDb -
                   measureTone(kRate, p, hpHz / 16).hpDb, 6, 1);
    }
    for (float rate : {44100.f, kRate, 96000.f, 192000.f}) {
        CHECK_NEAR(measureTone(rate, {1, 1}, 6000).lpDb, -3.0103, .5);
        CHECK_NEAR(measureTone(rate, {1, 1}, 4700).hpDb, -3.0103, .5);
    }

    const auto limited = measureTone(8000, {1, 1}, 3600);
    CHECK_NEAR(limited.lpDb, -3.0103, .5);
    CHECK_NEAR(limited.hpDb, -3.0103, .5);

    Filters923 module;
    Filters923::Outputs out;
    for (int i = 0; i < int(kRate); ++i)
        out = module.process(kRate, {0, 0}, {2, -3});
    CHECK_NEAR(out.lpOut, 2, 1e-6);
    CHECK_NEAR(out.hpOut, 0, 1e-6);

    Filters923 lpModule, hpModule;
    double lpLeak = 0, hpLeak = 0;
    for (int i = 0; i < int(kRate); ++i) {
        const float signal = float(std::sin(2 * kPi * 300 * i / kRate));
        const auto lpOnly = lpModule.process(kRate, {.2f, .8f}, {signal, 0});
        const auto hpOnly = hpModule.process(kRate, {.2f, .8f}, {0, signal});
        hpLeak = std::max(hpLeak, std::abs(double(lpOnly.hpOut)));
        lpLeak = std::max(lpLeak, std::abs(double(hpOnly.lpOut)));
    }
    CHECK_NEAR(hpLeak, 0, 1e-6);
    CHECK_NEAR(lpLeak, 0, 1e-6);
}

using Complex = std::complex<double>;

static void fft(std::vector<Complex>& data) {
    const auto size = data.size();
    for (std::size_t i = 1, reversed = 0; i < size; ++i) {
        std::size_t bit = size / 2;
        for (; reversed & bit; bit /= 2) reversed ^= bit;
        reversed ^= bit;
        if (i < reversed) std::swap(data[i], data[reversed]);
    }
    for (std::size_t length = 2; length <= size; length *= 2) {
        const Complex step = std::polar(1.0, -2 * kPi / length);
        for (std::size_t start = 0; start < size; start += length) {
            Complex phase = 1;
            for (std::size_t i = start; i < start + length / 2; ++i) {
                const auto even = data[i];
                const auto odd = phase * data[i + length / 2];
                data[i] = even + odd;
                data[i + length / 2] = even - odd;
                phase *= step;
            }
        }
    }
}

static double bandDensity(const std::vector<double>& powers, double binHz, double lo, double hi) {
    double sum = 0;
    int bins = 0;
    // Skip DC and average bin powers to compare spectral density across bandwidths.
    for (std::size_t i = 1; i < powers.size(); ++i) {
        const double hz = i * binHz;
        if (hz < lo || hz >= hi) continue;
        sum += powers[i];
        ++bins;
    }
    return sum / bins;
}

static void testSpectrum(float rate) {
    constexpr int size = 32768;
    constexpr int blocks = 48;
    const double binHz = rate / size;
    Filters923 module(923);
    std::vector<Complex> white(size), pink(size);
    std::vector<double> window(size), whitePower(size / 2), pinkPower(size / 2);
    for (int i = 0; i < size; ++i) {
        window[i] = .5 - .5 * std::cos(2 * kPi * i / size);
        module.process(rate, {}, {});
    }
    // Average Hann-windowed periodograms of disjoint blocks.
    for (int block = 0; block < blocks; ++block) {
        for (int i = 0; i < size; ++i) {
            const auto out = module.process(rate, {}, {});
            white[i] = window[i] * out.white;
            pink[i] = window[i] * out.pink;
        }
        fft(white);
        fft(pink);
        for (int i = 0; i < size / 2; ++i) {
            whitePower[i] += std::norm(white[i]);
            pinkPower[i] += std::norm(pink[i]);
        }
    }

    const double whiteReference = bandDensity(whitePower, binHz, 100, 10000);
    for (double lo = 100; lo < 10000; lo *= 2) {
        const double band = bandDensity(whitePower, binHz, lo, std::min(2 * lo, 10000.));
        CHECK_NEAR(10 * std::log10(band / whiteReference), 0, 1.5);
    }

    // Adjacent full octaves halve the density. Normalize the final partial octave
    // against the average of 1/f over the actual band edges.
    const auto pinkDensity = [&](double lo, double hi) {
        return bandDensity(pinkPower, binHz, lo, hi);
    };
    for (double lo = 50; lo * 4 <= 10000; lo *= 2)
        CHECK_NEAR(10 * std::log10(pinkDensity(2 * lo, 4 * lo) / pinkDensity(lo, 2 * lo)), -3, 1);
    const double expectedRatio = (std::log(10000. / 6400) / 3600) / (std::log(2.) / 3200);
    CHECK_NEAR(10 * std::log10(pinkDensity(6400, 10000) / pinkDensity(3200, 6400)),
               10 * std::log10(expectedRatio), 1);
    CHECK_NEAR(10 * std::log10(pinkDensity(40, 80) / pinkDensity(20, 40)), -3, 1);
    CHECK_NEAR(10 * std::log10(pinkDensity(10000, 20000) / pinkDensity(5000, 10000)), -3, 1);

    CHECK_NEAR(bandDensity(whitePower, binHz, 0, 3) < .45 * whiteReference, true, 0);
    CHECK_NEAR(pinkDensity(0, 3) < 1.25 * pinkDensity(5, 10), true, 0);
}

static void testNoiseStatistics(float rate) {
    constexpr std::size_t channels = 4;
    Filters923 a(17), b(18);
    std::array<double, channels> sum{}, square{};
    std::array<std::array<double, channels>, channels> product{};
    const int count = int(6 * rate);
    bool finite = true;
    for (int i = 0; i < count; ++i) {
        const float knob = (i % 2048) / 2047.f;
        const float input = i % 2 ? 1.f : -1.f;
        const auto first = a.process(rate, {knob, 1 - knob}, {input, -input});
        const auto second = b.process(rate, {}, {});
        finite &= std::isfinite(first.lpOut) && std::isfinite(first.hpOut);
        const std::array<double, channels> samples{first.white, first.pink, second.white, second.pink};
        for (std::size_t c = 0; c < channels; ++c) {
            const double sample = samples[c];
            finite &= std::isfinite(sample);
            sum[c] += sample;
            square[c] += sample * sample;
            for (std::size_t d = 0; d < c; ++d) product[c][d] += sample * samples[d];
        }
    }
    CHECK_NEAR(finite, true, 0);
    for (std::size_t c = 0; c < channels; ++c) {
        const double expectedRms = c % 2 ? 1.14 : .73;
        CHECK_NEAR(sum[c] / count, 0, .01);
        CHECK_NEAR(std::sqrt(square[c] / count), expectedRms, .03 * expectedRms);
        for (std::size_t d = 0; d < c; ++d) {
            const double varianceC = square[c] - sum[c] * sum[c] / count;
            const double varianceD = square[d] - sum[d] * sum[d] / count;
            const double covariance = product[c][d] - sum[c] * sum[d] / count;
            CHECK_NEAR(covariance / std::sqrt(varianceC * varianceD), 0, .04);
        }
    }
}

static void testSeed() {
    constexpr std::uint32_t seed = 0xffffffffu;
    Filters923 a(seed), b(seed), c;
    for (int i = 0; i < 1024; ++i) c.process(kRate, {}, {});
    c.seed(seed);
    bool sameWhite = true, samePink = true;
    for (int i = 0; i < 4096; ++i) {
        const auto first = a.process(kRate, {}, {});
        const auto second = b.process(kRate, {}, {});
        const auto reseeded = c.process(kRate, {}, {});
        sameWhite &= first.white == second.white && first.white == reseeded.white;
        samePink &= first.pink == second.pink && first.pink == reseeded.pink;
    }
    CHECK_NEAR(sameWhite, true, 0);
    CHECK_NEAR(samePink, true, 0);
}

int main() {
    testTaper();
    testFilters();
    testSeed();
    testSpectrum(44100.f);
    testSpectrum(kRate);
    for (float rate : {44100.f, kRate, 96000.f, 192000.f}) testNoiseStatistics(rate);
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
