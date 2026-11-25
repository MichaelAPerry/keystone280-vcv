#include <array>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <random>

#include "rack_shim.hpp"
#include "../src/input_voltage.hpp"
#include "build/wrappers.hpp"

// Per-instance CPU budget in percent of real time. It exists to catch denormal or
// runaway stalls (10x and more), not to benchmark. Shared CI machines are slower and
// noisier, so CI raises it with K280_CPU_BUDGET.
static double cpuBudget() {
    static const double budget = [] {
        const char* env = std::getenv("K280_CPU_BUDGET");
        return env ? std::atof(env) : 5.;
    }();
    return budget;
}

namespace {

constexpr int blockSize = 64;

float stimulus(int frame, int rate, std::mt19937& rng) {
    if (frame == 4 * rate) return std::numeric_limits<float>::quiet_NaN();
    if (frame == 4 * rate + 1) return std::numeric_limits<float>::infinity();
    if (frame == 4 * rate + 2) return -std::numeric_limits<float>::infinity();
    if (frame >= 6 * rate && frame < 6 * rate + rate / 2)
        return std::numeric_limits<float>::min() * ((frame % 2) ? 0.5f : -0.5f);
    if (frame < rate || frame >= 6 * rate) return 0.f;
    if (frame < 2 * rate) return std::uniform_real_distribution<float>(-10.f, 10.f)(rng);
    if (frame < 3 * rate) return 12.f;
    if (frame < 4 * rate) return -12.f;
    return ((frame / 5) % 2) ? 12.f : -12.f;
}

void sweep(Module& module, std::mt19937& rng) {
    for (auto& q : module.quantities) {
        const float value = q->snapEnabled
            ? float(std::uniform_int_distribution<int>(int(q->minValue), int(q->maxValue))(rng))
            : std::uniform_real_distribution<float>(q->minValue, q->maxValue)(rng);
        q->setImmediateValue(value);
    }
}

template<class T>
float outputLimit(std::size_t) { return 12.f; }

template<>
float outputLimit<Lackawanna_wrapper::LackawannaModule>(std::size_t port) {
    // A high-pass output is input minus its low-pass state. A rail reversal can
    // put both terms at the same polarity, briefly reaching nearly 24 V.
    return port == Lackawanna_wrapper::LackawannaModule::HP_OUT ? 24.f : 12.f;
}

template<class T, std::size_t Count>
void run(const char* name, int rate) {
    std::array<T, Count> modules;
    std::mt19937 rng(280);
    const float sampleTime = 1.f / rate;
    const int frames = rate * 10;
    for (auto& module : modules) module.onSampleRateChange({float(rate), sampleTime});
    float peak = 0.f;
    const auto start = std::clock();
    auto intervalStart = start;
    double worstPercent = 0.;
    for (auto& module : modules)
        for (auto& output : module.outputs)
            output.setVoltage(std::numeric_limits<float>::quiet_NaN());
    for (int frame = 0; frame < frames; ++frame) {
        for (auto& module : modules) {
            if (frame % blockSize == 0) {
                sweep(module, rng);
                for (auto& input : module.inputs) {
                    // Exercise normals in the last second. Channel 1 carries a decoy CV.
                    input.channels = frame < 9 * rate ? 2 : int(rng() % 2);
                    input.voltages[1] = input.channels == 2 ? 9.f : 0.f;
                }
                for (auto& output : module.outputs) output.channels = rng() % 2;
            }
            for (auto& input : module.inputs)
                input.voltages[0] = input.isConnected() ? stimulus(frame, rate, rng) : 0.f;

            module.process({float(rate), sampleTime, frame});
            for (std::size_t port = 0; port < module.outputs.size(); ++port) {
                const auto& output = module.outputs[port];
                const float volts = output.getVoltage();
                if (!std::isfinite(volts) || std::abs(volts) > outputLimit<T>(port)
                    || output.channels > 1 || output.getVoltage(1) != 0.f) {
                    std::cerr << name << " at " << rate << " Hz, frame " << frame
                              << ", output " << port << ": " << volts << " V\n";
                    std::exit(EXIT_FAILURE);
                }
                peak = std::max(peak, std::abs(volts));
            }
            for (const auto& light : module.lights) {
                if (!(light.brightness >= 0.f && light.brightness <= 1.f)) {
                    std::cerr << name << " at " << rate << " Hz, frame " << frame
                              << ": invalid light brightness\n";
                    std::exit(EXIT_FAILURE);
                }
            }
        }
        if ((frame + 1) % rate == 0) {
            const auto now = std::clock();
            const double percent = 100. * (now - intervalStart) / CLOCKS_PER_SEC / Count;
            worstPercent = std::max(worstPercent, percent);
            if (frame + 1 >= 7 * rate && percent >= cpuBudget()) {
                std::cerr << name << " at " << rate << " Hz, second " << (frame + 1) / rate
                          << ": " << percent << "% CPU per instance exceeds the " << cpuBudget() << "% budget\n";
                std::exit(EXIT_FAILURE);
            }
            intervalStart = now;
        }
    }
    const double percent = 100. * (intervalStart - start) / CLOCKS_PER_SEC / (10. * Count);
    if (percent >= cpuBudget()) {
        std::cerr << name << " at " << rate << " Hz: " << percent
                  << "% mean CPU per instance exceeds the " << cpuBudget() << "% budget\n";
        std::exit(EXIT_FAILURE);
    }
    std::cout << name << " " << rate << " Hz, " << Count << " instance(s): peak "
              << peak << " V, " << percent << "% mean / " << worstPercent
              << "% peak CPU per instance" << std::endl;
}

template<class T>
void test(const char* name) {
    for (int rate : {44100, 48000, 96000}) {
        run<T, 1>(name, rate);
        run<T, 8>(name, rate);
    }
}

} // namespace

int main() {
    test<Allegheny_wrapper::AlleghenyModule>("Allegheny");
    test<Brandywine_wrapper::BrandywineModule>("Brandywine");
    test<Clarion_wrapper::ClarionModule>("Clarion");
    test<Conestoga_wrapper::ConestogaModule>("Conestoga");
    test<Juniata_wrapper::JuniataModule>("Juniata");
    test<Kiski_wrapper::KiskiModule>("Kiski");
    test<Lackawanna_wrapper::LackawannaModule>("Lackawanna");
    test<Lehigh_wrapper::LehighModule>("Lehigh");
    test<Loyalsock_wrapper::LoyalsockModule>("Loyalsock");
    test<Monongahela_wrapper::MonongahelaModule>("Monongahela");
    test<Schuylkill_wrapper::SchuylkillModule>("Schuylkill");
    test<Susquehanna_wrapper::SusquehannaModule>("Susquehanna");
    test<Swatara_wrapper::SwataraModule>("Swatara");
    test<Tioga_wrapper::TiogaModule>("Tioga");
}
