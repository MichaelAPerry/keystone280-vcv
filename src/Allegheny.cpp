// VCV wrapper for the 904A low pass filter. DSP lives in engine/modules/Lpf904A.hpp.
#include "common.hpp"
#include "layout/Allegheny.hpp"
#include "modules/Lpf904A.hpp"

namespace L = layout_Allegheny;
using k280::Lpf904A;

struct AlleghenyModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { FIXED_CV, RANGE, REGEN, PARAMS_LEN };
    enum InputId { IN, CV1, CV2, CV3, INPUTS_LEN };
    enum OutputId { OUT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Lpf904A filter;

    struct FixedCvQuantity : ParamQuantity {
        std::string getString() override {
            if (!module)
                return ParamQuantity::getString();
            Lpf904A::Params p;
            p.fixedCv = getValue();
            p.range = int(std::round(module->params[RANGE].getValue()));
            const float hz = static_cast<AlleghenyModule*>(module)->filter.cutoffHz(p, 0.f);
            return getLabel() + string::f(": %.4g Hz cutoff (pole), %s", hz,
                                         k280vcv::clockString(getScaledValue()).c_str());
        }
    };

    AlleghenyModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<FixedCvQuantity>(FIXED_CV, 0.f, 1.f, 0.5f, "Fixed control voltage");
        configSwitch(RANGE, 0.f, 2.f, 1.f, "Frequency range",
                     {"1 (1 Hz–5 kHz)", "2 (4 Hz–20 kHz)", "3 (16 Hz–80 kHz)"});
        configParam<k280vcv::ClockQuantity>(REGEN, 0.f, 1.f, 0.f, "Regeneration", "", 0.f, 10.f);

        configInput(IN, "Signal in (DC coupled)");
        configInput(CV1, "Control 1 (1 V/oct)");
        configInput(CV2, "Control 2 (1 V/oct)");
        configInput(CV3, "Control 3 (1 V/oct)");
        configOutput(OUT, "Signal out (AC coupled)");
    }

    void onSampleRateChange(const SampleRateChangeEvent&) override { filter.reset(); }

    void process(const ProcessArgs& args) override {
        Lpf904A::Params p;
        p.fixedCv = params[FIXED_CV].getValue();
        p.range = int(std::round(params[RANGE].getValue()));
        p.regen = params[REGEN].getValue();

        Lpf904A::Inputs in;
        in.in = k280vcv::inputVoltage(inputs[IN]);
        in.cv[0] = k280vcv::inputVoltage(inputs[CV1]);
        in.cv[1] = k280vcv::inputVoltage(inputs[CV2]);
        in.cv[2] = k280vcv::inputVoltage(inputs[CV3]);

        outputs[OUT].setVoltage(filter.process(args.sampleRate, p, in).out);
    }
};

struct AlleghenyWidget : ModuleWidget {
    AlleghenyWidget(AlleghenyModule* module) {
        using M = AlleghenyModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Allegheny.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::FIXED_CV_X, L::FIXED_CV_Y)), module, M::FIXED_CV));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::RANGE_X, L::RANGE_Y)), module, M::RANGE,
                                                               L::RANGE_MIN_DEG, L::RANGE_MAX_DEG));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::REGEN_X, L::REGEN_Y)), module, M::REGEN));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN_X, L::IN_Y)), module, M::IN));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV1_X, L::CV1_Y)), module, M::CV1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV2_X, L::CV2_Y)), module, M::CV2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV3_X, L::CV3_Y)), module, M::CV3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT_X, L::OUT_Y)), module, M::OUT));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Lpf904A::kConfidence, "Cutoff and self-oscillation were bench measured. The ladder response is modeled.");
    }
};

Model* modelAllegheny = createModel<AlleghenyModule, AlleghenyWidget>("Allegheny");
