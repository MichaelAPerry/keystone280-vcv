// VCV wrapper for the 902 VCA. DSP lives in engine/modules/Vca902.hpp.
#include "common.hpp"
#include "layout/Schuylkill.hpp"
#include "modules/Vca902.hpp"

namespace L = layout_Schuylkill;
using k280::Vca902;

// Horizontal two-position rocker: value 0 = left, 1 = right. Reuses the left and right frames of
// the stock three-position horizontal switch.
struct RockerSwitch : app::SvgSwitch {
    RockerSwitch() {
        shadow->opacity = 0.f;
        addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThreeHorizontal_0.svg")));
        addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThreeHorizontal_2.svg")));
    }
};

struct SchuylkillModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { MODE, FIXED_CV, PARAMS_LEN };
    enum InputId { IN, IN2, CV1, CV2, CV3, INPUTS_LEN };
    enum OutputId { OUTN, OUT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Vca902 vca;

    SchuylkillModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configSwitch(MODE, 0.f, 1.f, 1.f, "Control mode", {"Linear", "Exponential"});
        configParam<k280vcv::ClockQuantity>(FIXED_CV, 0.f, 1.f, 0.f, "Fixed control voltage", " V", 0.f, 6.f);

        configInput(IN, "Signal in +");
        configInput(IN2, "Signal in −");
        configInput(CV1, "Control 1 (summed, −6 to +6 V)");
        configInput(CV2, "Control 2 (summed, −6 to +6 V)");
        configInput(CV3, "Control 3 (summed, −6 to +6 V)");
        configOutput(OUTN, "Signal out −");
        configOutput(OUT, "Signal out +");
    }

    void process(const ProcessArgs& args) override {
        Vca902::Params p;
        p.exp = params[MODE].getValue() > 0.5f;
        p.fixedCv = params[FIXED_CV].getValue();

        Vca902::Inputs in;
        in.in = k280vcv::inputVoltage(inputs[IN]);
        in.in2 = k280vcv::inputVoltage(inputs[IN2]);
        in.cv[0] = k280vcv::inputVoltage(inputs[CV1]);
        in.cv[1] = k280vcv::inputVoltage(inputs[CV2]);
        in.cv[2] = k280vcv::inputVoltage(inputs[CV3]);

        Vca902::Outputs o = vca.process(args.sampleRate, p, in);
        outputs[OUT].setVoltage(o.out);
        outputs[OUTN].setVoltage(o.outNeg);
    }
};

struct SchuylkillWidget : ModuleWidget {
    SchuylkillWidget(SchuylkillModule* module) {
        using M = SchuylkillModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Schuylkill.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<RockerSwitch>(mm2px(Vec(L::MODE_X, L::MODE_Y)), module, M::MODE));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::FIXED_CV_X, L::FIXED_CV_Y)), module, M::FIXED_CV));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN_X, L::IN_Y)), module, M::IN));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN2_X, L::IN2_Y)), module, M::IN2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV1_X, L::CV1_Y)), module, M::CV1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV2_X, L::CV2_Y)), module, M::CV2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CV3_X, L::CV3_Y)), module, M::CV3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUTN_X, L::OUTN_Y)), module, M::OUTN));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT_X, L::OUT_Y)), module, M::OUT));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Vca902::kConfidence, "Gain curves estimated from bench measurements.");
    }
};

Model* modelSchuylkill = createModel<SchuylkillModule, SchuylkillWidget>("Schuylkill");
