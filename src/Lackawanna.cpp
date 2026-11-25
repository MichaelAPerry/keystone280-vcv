// VCV wrapper for the 923 filters / noise. DSP lives in engine/modules/Filters923.hpp.
#include "common.hpp"
#include "layout/Lackawanna.hpp"
#include "modules/Filters923.hpp"

namespace L = layout_Lackawanna;
using k280::Filters923;

struct LackawannaModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { LP, HP, PARAMS_LEN };
    enum InputId { LP_IN, HP_IN, INPUTS_LEN };
    enum OutputId { LP_OUT, HP_OUT, WHITE1, WHITE2, PINK1, PINK2, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Filters923 filters;

    // Cutoff knob shown in Hz through the engine's dial table.
    struct CutoffQuantity : k280vcv::ClockQuantity {
        float getDisplayValue() override {
            static const Filters923 table;
            return table.cutoffHz(getValue(), paramId == HP);
        }
        void setDisplayValue(float d) override {
            if (!std::isfinite(d)) return;
            static const Filters923 table;
            float lo = 0.f, hi = 1.f;  // cutoffHz is monotonic in the knob
            for (int i = 0; i < 30; i++) {
                float mid = 0.5f * (lo + hi);
                (table.cutoffHz(mid, paramId == HP) < d ? lo : hi) = mid;
            }
            setImmediateValue(0.5f * (lo + hi));
        }
        std::string getUnit() override { return " Hz"; }
    };

    LackawannaModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<CutoffQuantity>(LP, 0.f, 1.f, 1.f, "Low pass cutoff");
        configParam<CutoffQuantity>(HP, 0.f, 1.f, 0.f, "High pass cutoff");

        configInput(LP_IN, "Low pass in (DC coupled)");
        configInput(HP_IN, "High pass in (DC coupled)");
        configOutput(LP_OUT, "Low pass out (6 dB/oct)");
        configOutput(HP_OUT, "High pass out (6 dB/oct)");
        configOutput(WHITE1, "White noise 1 (0 dBu)");
        configOutput(WHITE2, "White noise 2 (0 dBu)");
        configOutput(PINK1, "Pink noise 1 (+4 dBu)");
        configOutput(PINK2, "Pink noise 2 (+4 dBu)");

        // Each instance gets its own noise, so two 923s in one patch are uncorrelated.
        filters.seed(random::u32());
    }

    void process(const ProcessArgs& args) override {
        Filters923::Params p;
        p.lp = params[LP].getValue();
        p.hp = params[HP].getValue();

        Filters923::Inputs in;
        in.lpIn = inputs[LP_IN].getVoltage();
        in.hpIn = inputs[HP_IN].getVoltage();

        Filters923::Outputs o = filters.process(args.sampleRate, p, in);
        outputs[LP_OUT].setVoltage(o.lpOut);
        outputs[HP_OUT].setVoltage(o.hpOut);
        outputs[WHITE1].setVoltage(o.white);
        outputs[WHITE2].setVoltage(o.white);
        outputs[PINK1].setVoltage(o.pink);
        outputs[PINK2].setVoltage(o.pink);
    }
};

struct LackawannaWidget : ModuleWidget {
    LackawannaWidget(LackawannaModule* module) {
        using M = LackawannaModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Lackawanna.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::LP_X, L::LP_Y)), module, M::LP));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::HP_X, L::HP_Y)), module, M::HP));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::LP_IN_X, L::LP_IN_Y)), module, M::LP_IN));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::HP_IN_X, L::HP_IN_Y)), module, M::HP_IN));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::LP_OUT_X, L::LP_OUT_Y)), module, M::LP_OUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::HP_OUT_X, L::HP_OUT_Y)), module, M::HP_OUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::WHITE1_X, L::WHITE1_Y)), module, M::WHITE1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::WHITE2_X, L::WHITE2_Y)), module, M::WHITE2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::PINK1_X, L::PINK1_Y)), module, M::PINK1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::PINK2_X, L::PINK2_Y)), module, M::PINK2));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Filters923::kConfidence, "Filter cutoff at noon and noise output spectra were bench measured.");
    }
};

Model* modelLackawanna = createModel<LackawannaModule, LackawannaWidget>("Lackawanna");
