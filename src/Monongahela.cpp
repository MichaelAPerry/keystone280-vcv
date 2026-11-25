// VCV wrapper for the 904B high pass filter. DSP lives in engine/modules/Hpf904B.hpp.
#include "common.hpp"
#include "layout/Monongahela.hpp"
#include "modules/Hpf904B.hpp"

namespace L = layout_Monongahela;
using k280::Hpf904B;

struct MonongahelaModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { FIXED_CV, RANGE, PARAMS_LEN };
    enum InputId { IN, CV1, CV2, CV3, INPUTS_LEN };
    enum OutputId { OUT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Hpf904B filter;

    // Fixed control voltage in volts, with the resulting cutoff (no CV inputs) in the tooltip.
    struct FixedCvQuantity : k280vcv::ClockQuantity {
        std::string getString() override {
            std::string s = k280vcv::ClockQuantity::getString();
            if (!module) return s;
            Hpf904B::Params p;
            p.fixedCv = getValue();
            p.range = int(std::round(module->params[RANGE].getValue()));
            float hz = static_cast<MonongahelaModule*>(module)->filter.cutoffHz(p, 0.f);
            return s + string::f(", fc %.4g Hz", hz);
        }
    };

    MonongahelaModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<FixedCvQuantity>(FIXED_CV, 0.f, 1.f, 0.5f, "Fixed control voltage", " V", 0.f, 12.f, -6.f);
        configSwitch(RANGE, 0.f, 1.f, 0.f, "Frequency range", {"Low (<4 Hz–20 kHz)", "High (<10 Hz–50 kHz)"});

        configInput(IN, "Signal in (DC coupled)");
        configInput(CV1, "Control 1 (1 V/oct)");
        configInput(CV2, "Control 2 (1 V/oct)");
        configInput(CV3, "Control 3 (1 V/oct)");
        configOutput(OUT, "Signal out (DC coupled)");
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override { filter.reset(); }

    void process(const ProcessArgs& args) override {
        Hpf904B::Params p;
        p.fixedCv = params[FIXED_CV].getValue();
        p.range = int(std::round(params[RANGE].getValue()));

        Hpf904B::Inputs in;
        in.in = inputs[IN].getVoltage();
        in.cv[0] = inputs[CV1].getVoltage();
        in.cv[1] = inputs[CV2].getVoltage();
        in.cv[2] = inputs[CV3].getVoltage();

        outputs[OUT].setVoltage(filter.process(args.sampleRate, p, in).out);
    }
};

struct MonongahelaWidget : ModuleWidget {
    MonongahelaWidget(MonongahelaModule* module) {
        using M = MonongahelaModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Monongahela.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::FIXED_CV_X, L::FIXED_CV_Y)), module, M::FIXED_CV));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::RANGE_X, L::RANGE_Y)), module, M::RANGE,
                                                               L::RANGE_MIN_DEG, L::RANGE_MAX_DEG));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN_X, L::IN_Y)), module, M::IN));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV1_X, L::CV1_Y)), module, M::CV1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV2_X, L::CV2_Y)), module, M::CV2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV3_X, L::CV3_Y)), module, M::CV3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT_X, L::OUT_Y)), module, M::OUT));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Hpf904B::kConfidence, "Cutoff at noon was measured in each range. The four-pole response is modeled.");
    }
};

Model* modelMonongahela = createModel<MonongahelaModule, MonongahelaWidget>("Monongahela");
