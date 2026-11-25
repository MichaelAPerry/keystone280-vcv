// Minimal Rack API used by the audio-side classes. See README.md in this directory.
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace string {
template<class... Args>
std::string f(const char* format, Args... args) {
    char buffer[256];
    std::snprintf(buffer, sizeof buffer, format, args...);
    return buffer;
}
}

namespace math { using std::clamp; }
namespace rack { namespace random { inline std::uint32_t u32() { return 280; } } }
using namespace rack;

struct json_t {};
inline json_t* json_object() { return nullptr; }
inline json_t* json_boolean(bool) { return nullptr; }
inline json_t* json_integer(int) { return nullptr; }
inline json_t* json_string(const char*) { return nullptr; }
inline void json_object_set_new(json_t*, const char*, json_t*) {}
inline json_t* json_object_get(json_t*, const char*) { return nullptr; }
inline bool json_is_true(json_t*) { return false; }
inline bool json_is_boolean(json_t*) { return false; }
inline bool json_is_integer(json_t*) { return false; }
inline int json_integer_value(json_t*) { return 0; }
inline const char* json_string_value(json_t*) { return nullptr; }

struct Param {
    float value = 0.f;
    float getValue() const { return value; }
};

struct Port {
    int channels = 0;
    float voltages[16]{};
    bool isConnected() const { return channels > 0; }
    float getVoltage(int channel = 0) const { return voltages[channel]; }
    void setVoltage(float voltage, int channel = 0) { voltages[channel] = voltage; }
};

struct Light {
    float brightness = 0.f;
    void setBrightness(float value) { brightness = value; }
};

struct Module;
struct ParamQuantity {
    Module* module = nullptr;
    int paramId = 0;
    float minValue = 0.f, maxValue = 1.f;
    bool snapEnabled = false;
    virtual ~ParamQuantity() = default;
    virtual std::string getString() { return {}; }
    virtual std::string getLabel() { return {}; }
    virtual std::string getUnit() { return {}; }
    virtual std::string getDisplayValueString() { return {}; }
    virtual float getDisplayValue() { return getValue(); }
    virtual void setDisplayValue(float value) { setImmediateValue(value); }
    float getValue() const;
    float getScaledValue() const { return (getValue() - minValue) / (maxValue - minValue); }
    void setImmediateValue(float value);
};

struct Module {
    struct ProcessArgs { float sampleRate, sampleTime; std::int64_t frame; };
    struct SampleRateChangeEvent { float sampleRate, sampleTime; };

    std::vector<Param> params;
    std::vector<std::unique_ptr<ParamQuantity>> quantities;
    std::vector<Port> inputs, outputs;
    std::vector<Light> lights;

    virtual ~Module() = default;
    virtual void process(const ProcessArgs&) {}
    virtual void onSampleRateChange(const SampleRateChangeEvent&) {}
    virtual void onReset() {}
    virtual json_t* dataToJson() { return nullptr; }
    virtual void dataFromJson(json_t*) {}

    void config(int paramCount, int inputCount, int outputCount, int lightCount) {
        params.resize(paramCount);
        quantities.resize(paramCount);
        inputs.resize(inputCount);
        outputs.resize(outputCount);
        lights.resize(lightCount);
    }
    template<class T = ParamQuantity>
    T* configParam(int id, float low, float high, float initial, const std::string&,
                   const std::string& = {}, float = 0.f, float = 1.f, float = 0.f) {
        auto quantity = std::make_unique<T>();
        T* result = quantity.get();
        result->module = this;
        result->paramId = id;
        result->minValue = low;
        result->maxValue = high;
        result->setImmediateValue(initial);
        quantities.at(id) = std::move(quantity);
        return result;
    }
    void configSwitch(int id, float low, float high, float initial, const std::string& name,
                      std::initializer_list<const char*>) {
        configParam(id, low, high, initial, name)->snapEnabled = true;
    }
    void configButton(int id, const std::string& name) {
        configSwitch(id, 0.f, 1.f, 0.f, name, {});
    }
    void configInput(int, const std::string&) {}
    void configOutput(int, const std::string&) {}
    void configLight(int, const std::string&) {}
};

inline float ParamQuantity::getValue() const { return module->params[paramId].getValue(); }
inline void ParamQuantity::setImmediateValue(float value) {
    module->params[paramId].value = std::clamp(value, minValue, maxValue);
}

namespace k280vcv {
inline std::string clockString(float) { return {}; }
struct ClockQuantity : ParamQuantity {};
}
