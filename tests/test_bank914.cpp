// Golden tests for the 914 fixed filter bank in m4-specs.md.
#include <algorithm>
#include <chrono>
#include <complex>
#include <vector>

#include "modules/Bank914.hpp"
#include "test_main.hpp"

using k280::Bank914;

static constexpr float kRate = 48000.f;
static constexpr double kPi = 3.14159265358979323846;
static constexpr double kCentres[] = {
    125, 175, 250, 350, 500, 700, 1000, 1400, 2000, 2800, 4000, 5600,
};

static Bank914::Params allMax() {
    Bank914::Params p;
    p.lpLevel = p.hpLevel = 1.f;
    for (float& knob : p.band) knob = 1.f;
    return p;
}

static void testTaper() {
    Bank914 bank;
    CHECK_NEAR(bank.branchGainDb(-1), -65, 0);
    CHECK_NEAR(bank.branchGainDb(0), -65, 1);
    CHECK_NEAR(bank.branchGainDb(.2f), -16.9, 1);
    CHECK_NEAR(bank.branchGainDb(.5f), -7.9, 1);
    CHECK_NEAR(bank.branchGainDb(.8f), -3, 1);
    CHECK_NEAR(bank.branchGainDb(1), 0, 1);
    CHECK_NEAR(bank.branchGainDb(2), 0, 0);
    CHECK_NEAR(bank.branchGainDb(.35f), -12.4, 1e-5);

    bank.cal.levelTable[2] = {.52f, -8.f};
    CHECK_NEAR(bank.branchGainDb(.52f), -8, 0);
    CHECK_NEAR(bank.branchGainDb(.36f), -12.45, 1e-5);

    // Editing a dial mark must update a stationary knob's gain on the next sample.
    Bank914 edited, reference;
    Bank914::Params p;
    p.lpLevel = .5f;
    for (int i = 0; i < 480; ++i) {
        const Bank914::Inputs in{i == 0 ? 1.f : 0.f};
        edited.process(kRate, p, in);
        reference.process(kRate, p, in);
    }
    edited.cal.levelTable[2].db = 0.f;
    p.lpLevel = 1.f;
    const auto expected = reference.process(kRate, p, {});
    p.lpLevel = .5f;
    CHECK_NEAR(edited.process(kRate, p, {}).out, expected.out, 1e-9);
}

static std::vector<float> impulseResponse(const Bank914::Params& p, float rate = kRate) {
    Bank914 bank;
    // One second includes more than 12 time constants of the 2 Hz input coupling.
    std::vector<float> samples(static_cast<int>(rate));
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = bank.process(rate, p, {i == 0 ? 1.f : 0.f}).out;
    return samples;
}

static double responseDb(const std::vector<float>& impulse, double hz, float rate = kRate) {
    const auto step = std::polar(1.0, -2 * kPi * hz / rate);
    std::complex<double> response;
    // Evaluate the DTFT by Horner's rule at the precise probe frequency.
    for (auto i = impulse.rbegin(); i != impulse.rend(); ++i)
        response = response * step + double(*i);
    return 20 * std::log10(std::abs(response));
}

static double halfPowerHz(const std::vector<float>& impulse, double lo, double hi,
                          double targetDb, bool rising) {
    for (int i = 0; i < 18; ++i) {
        const double mid = (lo + hi) / 2;
        if ((responseDb(impulse, mid) < targetDb) == rising) lo = mid;
        else hi = mid;
    }
    return (lo + hi) / 2;
}

static void testBandPasses() {
    for (int i = 0; i < 12; ++i) {
        const double fc = kCentres[i];
        CHECK_NEAR(Bank914::kBandHz[i], fc, 0);
        Bank914::Params p;
        p.band[i] = 1.f;
        const auto impulse = impulseResponse(p);

        double lo = .8 * fc;
        double hi = 1.2 * fc;
        for (int search = 0; search < 20; ++search) {
            const double left = (2 * lo + hi) / 3;
            const double right = (lo + 2 * hi) / 3;
            if (responseDb(impulse, left) < responseDb(impulse, right)) lo = left;
            else hi = right;
        }
        const double peak = (lo + hi) / 2;
        CHECK_NEAR(peak, fc, .02 * fc);
        CHECK_NEAR(responseDb(impulse, fc), 0, .05);
        const double halfPowerDb = responseDb(impulse, peak) - 10 * std::log10(2.0);
        const double lower = halfPowerHz(impulse, .5 * fc, peak, halfPowerDb, true);
        const double upper = halfPowerHz(impulse, peak, 2 * fc, halfPowerDb, false);
        CHECK_NEAR(upper - lower, fc / 7, .1 * fc / 7);
    }
}

static void testEdgeBranches() {
    Bank914::Params p;
    p.lpLevel = 1.f;
    const auto lp = impulseResponse(p);
    CHECK_NEAR(responseDb(lp, 100), 0, .05);
    CHECK_NEAR(responseDb(lp, 400) - responseDb(lp, 200), -24, 1);

    p.lpLevel = 0.f;
    p.hpLevel = 1.f;
    // Use the higher sample rate for the slope to reduce bilinear warping.
    constexpr float rate = 192000.f;
    const auto hp = impulseResponse(p, rate);
    CHECK_NEAR(responseDb(impulseResponse(p), 7500), 0, .05);
    CHECK_NEAR(responseDb(hp, 3750, rate) - responseDb(hp, 1875, rate), 24, 1);
}

static void testCombinedResponse() {
    const auto maximum = impulseResponse(allMax());
    const auto minimum = impulseResponse({});
    double loudestMuted = responseDb(minimum, .02) - responseDb(maximum, .02);
    std::puts("914 all knobs at maximum (48 kHz)");
    std::puts("    Hz      dB");
    for (int i = 0; i <= 60; ++i) {
        // Anchor the 1/6-octave grid at 20 Hz and include the exact 20 kHz endpoint.
        const double hz = std::min(20000.0, 20 * std::exp2(i / 6.0));
        const double db = responseDb(maximum, hz);
        std::printf("%8.2f %7.2f\n", hz, db);
        CHECK_NEAR(std::isfinite(db), true, 0);
    }
    for (double hz : kCentres) {
        const double db = responseDb(maximum, hz);
        CHECK_NEAR(db >= -6 && db <= 3, true, 0);
    }
    for (int i = 1; i <= 1200; ++i) {
        const double hz = .02 * std::exp2(i * std::log2(1200000.0) / 1200);
        loudestMuted = std::max(loudestMuted, responseDb(minimum, hz) - responseDb(maximum, hz));
    }
    CHECK_NEAR(loudestMuted, -65, .05);
}

static void testCouplingAndLimit() {
    Bank914 dc;
    const auto p = allMax();
    Bank914::Outputs out;
    for (int i = 0; i < int(1.25f * kRate); ++i)
        out = dc.process(kRate, p, {2.f});
    CHECK_NEAR(out.out, 0, 1e-5);
    CHECK_NEAR(out.inThru, 2, 0);

    Bank914::Params lpOnly;
    lpOnly.lpLevel = 1.f;
    const auto lp = impulseResponse(lpOnly);
    CHECK_NEAR(responseDb(lp, 2) - responseDb(lp, 20), -2.97, .05);

    for (float rate : {44100.f, kRate, 96000.f, 192000.f, 16000.f, 8000.f}) {
        Bank914 bank, linearReference;
        bool finite = true;
        bool thruMatches = true;
        double peak = 0.0;
        double kneeError = 0.0;
        int kneeSamples = 0;
        for (int i = 0; i < int(rate / 5); ++i) {
            const float input = float(14 * std::sin(2 * kPi * 50 * i / rate));
            out = bank.process(rate, p, {input});
            finite &= std::isfinite(out.out);
            thruMatches &= out.inThru == input;
            peak = std::max(peak, std::fabs(double(out.out)));
            const float linear = 16.f * linearReference.process(rate, p, {input / 16.f}).out;
            const float magnitude = std::abs(linear);
            if (magnitude > 3.18f && magnitude < 3.22f) {
                ++kneeSamples;
                const double expected = 3.0 + .46 * std::tanh((magnitude - 3.0) / .46);
                kneeError = std::max(kneeError, std::abs(std::abs(double(out.out)) - expected));
            }
        }
        CHECK_NEAR(finite, true, 0);
        CHECK_NEAR(thruMatches, true, 0);
        CHECK_NEAR(peak <= 3.46f && peak > 3.f, true, 0);
        CHECK_NEAR(kneeSamples > 0, true, 0);
        CHECK_NEAR(kneeError, 0, 1e-5);
    }
}

static void testCpu() {
    Bank914 bank;
    const auto p = allMax();
    volatile float sink = 0.f;
    constexpr int count = 1 << 20;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < count; ++i) {
        const float input = (i % 256 - 128) * (1.f / 128);
        sink = bank.process(kRate, p, {input}).out;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double ns = std::chrono::duration<double, std::nano>(elapsed).count() / count;
    std::printf("914 CPU at 48 kHz: %.1f ns/sample\n", ns);
    CHECK_NEAR(ns < 400 && std::isfinite(sink), true, 0);
}

int main() {
    testTaper();
    testBandPasses();
    testEdgeBranches();
    testCombinedResponse();
    testCouplingAndLimit();
    testCpu();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
