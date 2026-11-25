// VCV wrapper for the 914 fixed filter bank. DSP lives in engine/modules/Bank914.hpp.
#include "common.hpp"
#include "layout/Brandywine.hpp"
#include "modules/Bank914.hpp"

namespace L = layout_Brandywine;
using k280::Bank914;

struct BrandywineModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId {
        LP_LEVEL, BAND1, BAND2, BAND3, BAND4, BAND5, BAND6, BAND7, BAND8, BAND9, BAND10, BAND11, BAND12,
        HP_LEVEL,
        PARAMS_LEN
    };
    enum InputId { IN, INPUTS_LEN };
    enum OutputId { IN_THRU, OUT, OUT2, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Bank914 bank;

    // Branch level knob shown in dB through the engine's dial table.
    struct LevelQuantity : k280vcv::ClockQuantity {
        float getDisplayValue() override {
            static const Bank914 table;
            return table.branchGainDb(getValue());
        }
        void setDisplayValue(float d) override {
            if (!std::isfinite(d)) return;
            static const Bank914 table;
            float lo = 0.f, hi = 1.f;  // branchGainDb is monotonic in the knob
            for (int i = 0; i < 30; i++) {
                float mid = 0.5f * (lo + hi);
                (table.branchGainDb(mid) < d ? lo : hi) = mid;
            }
            setImmediateValue(0.5f * (lo + hi));
        }
        std::string getUnit() override { return " dB"; }
    };

    BrandywineModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<LevelQuantity>(LP_LEVEL, 0.f, 1.f, 0.f, "100 Hz low pass level");
        for (int i = 0; i < 12; i++) {
            float hz = Bank914::kBandHz[i];
            configParam<LevelQuantity>(BAND1 + i, 0.f, 1.f, 0.f,
                                       hz < 1000.f ? string::f("%.0f Hz band", hz) : string::f("%g kHz band", hz / 1000.f));
        }
        configParam<LevelQuantity>(HP_LEVEL, 0.f, 1.f, 0.f, "7.5 kHz high pass level");

        configInput(IN, "Signal in (AC coupled)");
        configOutput(IN_THRU, "Signal in (parallel jack, raw input)");
        configOutput(OUT, "Signal out (AC coupled)");
        configOutput(OUT2, "Signal out (parallel jack)");
    }

    void onSampleRateChange(const SampleRateChangeEvent&) override { bank = Bank914{}; }

    void process(const ProcessArgs& args) override {
        Bank914::Params p;
        p.lpLevel = params[LP_LEVEL].getValue();
        for (int i = 0; i < 12; i++) p.band[i] = params[BAND1 + i].getValue();
        p.hpLevel = params[HP_LEVEL].getValue();

        Bank914::Inputs in;
        in.in = k280vcv::inputVoltage(inputs[IN]);

        Bank914::Outputs o = bank.process(args.sampleRate, p, in);
        outputs[IN_THRU].setVoltage(o.inThru);
        outputs[OUT].setVoltage(o.out);
        outputs[OUT2].setVoltage(o.out);
    }
};

struct BrandywineWidget : ModuleWidget {
    BrandywineWidget(BrandywineModule* module) {
        using M = BrandywineModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Brandywine.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::LP_LEVEL_X, L::LP_LEVEL_Y)), module, M::LP_LEVEL));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND1_X, L::BAND1_Y)), module, M::BAND1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND2_X, L::BAND2_Y)), module, M::BAND2));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND3_X, L::BAND3_Y)), module, M::BAND3));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND4_X, L::BAND4_Y)), module, M::BAND4));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND5_X, L::BAND5_Y)), module, M::BAND5));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND6_X, L::BAND6_Y)), module, M::BAND6));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND7_X, L::BAND7_Y)), module, M::BAND7));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND8_X, L::BAND8_Y)), module, M::BAND8));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND9_X, L::BAND9_Y)), module, M::BAND9));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND10_X, L::BAND10_Y)), module, M::BAND10));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND11_X, L::BAND11_Y)), module, M::BAND11));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::BAND12_X, L::BAND12_Y)), module, M::BAND12));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::HP_LEVEL_X, L::HP_LEVEL_Y)), module, M::HP_LEVEL));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN_X, L::IN_Y)), module, M::IN));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::IN_THRU_X, L::IN_THRU_Y)), module, M::IN_THRU));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT_X, L::OUT_Y)), module, M::OUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT2_X, L::OUT2_Y)), module, M::OUT2));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Bank914::kConfidence, "The 1 kHz band taper was measured. Other band responses use the same taper.");
    }
};

Model* modelBrandywine = createModel<BrandywineModule, BrandywineWidget>("Brandywine");
