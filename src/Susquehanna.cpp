// VCV wrapper for the 921 VCO. DSP lives in engine/modules/Vco921.hpp.
#include <atomic>

#include "common.hpp"
#include "layout/Susquehanna.hpp"
#include "modules/Vco921.hpp"

namespace L = layout_Susquehanna;
using k280::Vco921;

struct SusquehannaModule : Module {
    enum ParamId { FREQUENCY, RANGE, SCALE, COARSE, WIDTH, AUX_WAVE, CLAMP, AUX_LEVEL, PARAMS_LEN };
    enum InputId { PW1, PW2, VTRIG, STRIG, FCV1, FCV2, FCV3, INPUTS_LEN };
    enum OutputId { AUXN, AUX, SINE, TRI, SAW, RECT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Vco921 vco;
    // Set from the UI thread (context menu), read on the audio thread.
    std::atomic<bool> unitB{false};
    std::atomic<Vco921::ClampMode> clampMode{Vco921::ClampMode::Hold};

    // FREQUENCY shows octaves or semitones, following the SCALE switch.
    struct FrequencyQuantity : k280vcv::ClockQuantity {
        bool semis() const { return module && module->params[SCALE].getValue() > 0.5f; }
        float getDisplayValue() override { return (getValue() - 0.5f) * (semis() ? 24.f : 12.f); }
        void setDisplayValue(float d) override {
            if (std::isfinite(d)) setImmediateValue(math::clamp(d / (semis() ? 24.f : 12.f) + 0.5f, 0.f, 1.f));
        }
        std::string getUnit() override { return semis() ? " semitones" : " oct"; }
    };

    SusquehannaModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<FrequencyQuantity>(FREQUENCY, 0.f, 1.f, 0.5f, "Frequency");
        configSwitch(RANGE, 0.f, 5.f, 2.f, "Range", {"32'", "16'", "8'", "4'", "2'", "1'"});
        configSwitch(SCALE, 0.f, 1.f, 0.f, "Scale (frequency knob span)", {"±6 octaves", "±12 semitones"});
        configSwitch(COARSE, 0.f, 1.f, 0.f, "Coarse range", {"Audio", "Sub-audio"});
        configParam<k280vcv::ClockQuantity>(WIDTH, 0.f, 1.f, 0.5f, "Rectangular width", "%", 0.f, 80.f, 10.f);
        configSwitch(AUX_WAVE, 0.f, 5.f, 0.f, "Aux out waveform",
                     {"Sine", "Triangle", "Inverted sawtooth", "Sawtooth", "Rectangular", "Inverted rectangular"});
        configParam<k280vcv::ClockQuantity>(CLAMP, 0.f, 1.f, 0.5f, "Clamping point", "%", 0.f, 80.f, 10.f);
        configParam<k280vcv::ClockQuantity>(AUX_LEVEL, 0.f, 1.f, 0.f, "Aux out level", "", 0.f, 10.f);

        configInput(PW1, "Width CV 1 (1 V per 15 %)");
        configInput(PW2, "Width CV 2 (1 V per 15 %)");
        configInput(VTRIG, "Clamp V-trig");
        configInput(STRIG, "Clamp S-trig (active low)");
        configInput(FCV1, "Freq CV 1 (1 V/oct)");
        configInput(FCV2, "Freq CV 2 (1 V/oct)");
        configInput(FCV3, "Freq CV 3 (1 V/oct)");
        configOutput(AUXN, "Aux out −");
        configOutput(AUX, "Aux out +");
        configOutput(SINE, "Sine");
        configOutput(TRI, "Triangle");
        configOutput(SAW, "Sawtooth");
        configOutput(RECT, "Rectangle");
    }

    void process(const ProcessArgs& args) override {
        Vco921::Params p;
        p.frequency = params[FREQUENCY].getValue();
        p.range = int(std::round(params[RANGE].getValue()));
        p.semitoneScale = params[SCALE].getValue() > 0.5f;
        p.subAudio = params[COARSE].getValue() > 0.5f;
        p.width = params[WIDTH].getValue();
        p.auxWave = int(std::round(params[AUX_WAVE].getValue()));
        p.clampPoint = params[CLAMP].getValue();
        p.auxLevel = params[AUX_LEVEL].getValue();
        p.clampMode = clampMode.load(std::memory_order_relaxed);

        Vco921::Inputs in;
        in.fcv[0] = inputs[FCV1].getVoltage();
        in.fcv[1] = inputs[FCV2].getVoltage();
        in.fcv[2] = inputs[FCV3].getVoltage();
        in.pw[0] = inputs[PW1].getVoltage();
        in.pw[1] = inputs[PW2].getVoltage();
        in.vtrig = inputs[VTRIG].getVoltage();
        in.vtrigConnected = inputs[VTRIG].isConnected();
        in.strig = inputs[STRIG].getVoltage();
        in.strigConnected = inputs[STRIG].isConnected();

        vco.cal.unitOffsetSemitones = unitB ? k280::Vco921Calibration::unitBOffsetSemitones : 0.f;
        Vco921::Outputs o = vco.process(args.sampleRate, p, in);
        outputs[SINE].setVoltage(o.sine);
        outputs[TRI].setVoltage(o.tri);
        outputs[SAW].setVoltage(o.saw);
        outputs[RECT].setVoltage(o.rect);
        outputs[AUX].setVoltage(o.aux);
        outputs[AUXN].setVoltage(o.auxNeg);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "clampMode", json_string(clampMode == Vco921::ClampMode::Hold ? "hold" : "reset"));
        json_object_set_new(root, "unitB", json_boolean(unitB));
        return root;
    }

    void dataFromJson(json_t* root) override {
        unitB = json_is_true(json_object_get(root, "unitB"));
        const char* m = json_string_value(json_object_get(root, "clampMode"));  // null if absent or not a string
        clampMode = (m && std::string(m) == "reset") ? Vco921::ClampMode::Reset : Vco921::ClampMode::Hold;
    }
};

struct SusquehannaWidget : ModuleWidget {
    SusquehannaWidget(SusquehannaModule* module) {
        using M = SusquehannaModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Susquehanna.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::FREQUENCY_X, L::FREQUENCY_Y)), module, M::FREQUENCY));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::RANGE_X, L::RANGE_Y)), module, M::RANGE,
                                                               L::RANGE_MIN_DEG, L::RANGE_MAX_DEG));
        addParam(createParamCentered<CKSS>(mm2px(Vec(L::SCALE_X, L::SCALE_Y)), module, M::SCALE));
        addParam(createParamCentered<CKSS>(mm2px(Vec(L::COARSE_X, L::COARSE_Y)), module, M::COARSE));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::WIDTH_X, L::WIDTH_Y)), module, M::WIDTH));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::AUX_WAVE_X, L::AUX_WAVE_Y)), module,
                                                               M::AUX_WAVE, L::AUX_WAVE_MIN_DEG, L::AUX_WAVE_MAX_DEG));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::CLAMP_X, L::CLAMP_Y)), module, M::CLAMP));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::AUX_LEVEL_X, L::AUX_LEVEL_Y)), module, M::AUX_LEVEL));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::PW1_X, L::PW1_Y)), module, M::PW1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::PW2_X, L::PW2_Y)), module, M::PW2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::VTRIG_X, L::VTRIG_Y)), module, M::VTRIG));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::STRIG_X, L::STRIG_Y)), module, M::STRIG));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::FCV1_X, L::FCV1_Y)), module, M::FCV1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::FCV2_X, L::FCV2_Y)), module, M::FCV2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::FCV3_X, L::FCV3_Y)), module, M::FCV3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::AUXN_X, L::AUXN_Y)), module, M::AUXN));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::AUX_X, L::AUX_Y)), module, M::AUX));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SINE_X, L::SINE_Y)), module, M::SINE));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::TRI_X, L::TRI_Y)), module, M::TRI));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SAW_X, L::SAW_Y)), module, M::SAW));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::RECT_X, L::RECT_Y)), module, M::RECT));
    }

    void appendContextMenu(Menu* menu) override {
        auto* m = getModule<SusquehannaModule>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexSubmenuItem(
            "Clamp behaviour", {"Hold at clamping point while triggered", "Reset to clamping point on trigger"},
            [=]() { return m->clampMode == Vco921::ClampMode::Hold ? 0 : 1; },
            [=](int i) { m->clampMode = i == 0 ? Vco921::ClampMode::Hold : Vco921::ClampMode::Reset; }));
        menu->addChild(createIndexSubmenuItem(
            "Oscillator calibration", {"Unit A", "Unit B"},
            [=]() { return int(m->unitB.load()); },
            [=](int i) { m->unitB = i == 1; }));
        k280vcv::appendConfidence(menu, Vco921::kConfidence,
                                  "Pitch, range and output levels were bench measured. Waveform shapes are estimated across pitches.");
    }
};

Model* modelSusquehanna = createModel<SusquehannaModule, SusquehannaWidget>("Susquehanna");
