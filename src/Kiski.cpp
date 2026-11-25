// VCV wrapper for the CP35 attenuators / multiples / +-6 V. DSP lives in engine/modules/Atten35.hpp.
#include "common.hpp"
#include "layout/Kiski.hpp"
#include "modules/Atten35.hpp"

namespace L = layout_Kiski;
using k280::Atten35;

struct KiskiModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { ATT1, ATT2, ATT3, ATT4, PARAMS_LEN };
    enum InputId { AIN1, AIN2, AIN3, AIN4, MA_IN, MB_IN, INPUTS_LEN };
    enum OutputId {
        AOUT1, AOUT2, AOUT3, AOUT4, N6A, N6B, P6A, P6B, MA_OUT2, MA_OUT3, MA_OUT4, MB_OUT2, MB_OUT3,
        MB_OUT4,
        OUTPUTS_LEN
    };
    enum LightId { LIGHTS_LEN };

    Atten35 atten;

    // Attenuation shown as percent of the input (0 % = off, 100 % = unity).
    struct AttenQuantity : k280vcv::ClockQuantity {
        static float lookup(float knob) {
            static const Atten35 table;
            const auto& t = table.cal.taperTable;
            if (knob <= t.front().knob) return t.front().value;
            for (size_t i = 1; i < t.size(); i++) {
                if (knob <= t[i].knob) {
                    float f = (knob - t[i - 1].knob) / (t[i].knob - t[i - 1].knob);
                    return t[i - 1].value + f * (t[i].value - t[i - 1].value);
                }
            }
            return t.back().value;
        }
        float getDisplayValue() override { return lookup(getValue()) * 100.f; }
        void setDisplayValue(float d) override {
            if (!std::isfinite(d)) return;
            float lo = 0.f, hi = 1.f;  // table is monotonic in the knob
            for (int i = 0; i < 30; i++) {
                float mid = 0.5f * (lo + hi);
                (lookup(mid) * 100.f < d ? lo : hi) = mid;
            }
            setImmediateValue(0.5f * (lo + hi));
        }
        std::string getUnit() override { return "%"; }
    };

    KiskiModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 4; i++) configParam<AttenQuantity>(ATT1 + i, 0.f, 1.f, 1.f, string::f("Attenuator %d", i + 1));

        for (int i = 0; i < 4; i++)
            configInput(AIN1 + i, i == 0 ? std::string("Attenuator 1 in (feeds 2-4 when they are unpatched)")
                                         : string::f("Attenuator %d in (half-normalled from the one above)", i + 1));
        configInput(MA_IN, "Multiple A in");
        configInput(MB_IN, "Multiple B in");
        for (int i = 0; i < 4; i++) configOutput(AOUT1 + i, string::f("Attenuator %d out", i + 1));
        configOutput(N6A, "-6 V (A)");
        configOutput(N6B, "-6 V (B)");
        configOutput(P6A, "+6 V (A)");
        configOutput(P6B, "+6 V (B)");
        for (int i = 0; i < 3; i++) configOutput(MA_OUT2 + i, string::f("Multiple A out %d (copy of A in)", i + 2));
        for (int i = 0; i < 3; i++) configOutput(MB_OUT2 + i, string::f("Multiple B out %d (copy of B in)", i + 2));
    }

    void process(const ProcessArgs& args) override {
        Atten35::Params p;
        for (int i = 0; i < 4; i++) p.att[i] = params[ATT1 + i].getValue();

        Atten35::Inputs in;
        for (int i = 0; i < 4; i++) {
            in.ain[i] = inputs[AIN1 + i].getVoltage();
            in.ainConnected[i] = inputs[AIN1 + i].isConnected();
        }
        in.maIn = inputs[MA_IN].getVoltage();
        in.mbIn = inputs[MB_IN].getVoltage();

        Atten35::Outputs o = atten.process(args.sampleRate, p, in);
        for (int i = 0; i < 4; i++) outputs[AOUT1 + i].setVoltage(o.aout[i]);
        outputs[N6A].setVoltage(o.n6);
        outputs[N6B].setVoltage(o.n6);
        outputs[P6A].setVoltage(o.p6);
        outputs[P6B].setVoltage(o.p6);
        for (int i = 0; i < 3; i++) {
            outputs[MA_OUT2 + i].setVoltage(o.ma);
            outputs[MB_OUT2 + i].setVoltage(o.mb);
        }
    }
};

struct KiskiWidget : ModuleWidget {
    KiskiWidget(KiskiModule* module) {
        using M = KiskiModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Kiski.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ATT1_X, L::ATT1_Y)), module, M::ATT1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ATT2_X, L::ATT2_Y)), module, M::ATT2));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ATT3_X, L::ATT3_Y)), module, M::ATT3));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ATT4_X, L::ATT4_Y)), module, M::ATT4));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::AIN1_X, L::AIN1_Y)), module, M::AIN1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::AIN2_X, L::AIN2_Y)), module, M::AIN2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::AIN3_X, L::AIN3_Y)), module, M::AIN3));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::AIN4_X, L::AIN4_Y)), module, M::AIN4));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::MA_IN_X, L::MA_IN_Y)), module, M::MA_IN));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::MB_IN_X, L::MB_IN_Y)), module, M::MB_IN));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::AOUT1_X, L::AOUT1_Y)), module, M::AOUT1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::AOUT2_X, L::AOUT2_Y)), module, M::AOUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::AOUT3_X, L::AOUT3_Y)), module, M::AOUT3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::AOUT4_X, L::AOUT4_Y)), module, M::AOUT4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::N6A_X, L::N6A_Y)), module, M::N6A));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::N6B_X, L::N6B_Y)), module, M::N6B));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::P6A_X, L::P6A_Y)), module, M::P6A));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::P6B_X, L::P6B_Y)), module, M::P6B));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MA_OUT2_X, L::MA_OUT2_Y)), module, M::MA_OUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MA_OUT3_X, L::MA_OUT3_Y)), module, M::MA_OUT3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MA_OUT4_X, L::MA_OUT4_Y)), module, M::MA_OUT4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MB_OUT2_X, L::MB_OUT2_Y)), module, M::MB_OUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MB_OUT3_X, L::MB_OUT3_Y)), module, M::MB_OUT3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MB_OUT4_X, L::MB_OUT4_Y)), module, M::MB_OUT4));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Atten35::kConfidence, "The attenuation curve uses bench measurements from the first channel.");
    }
};

Model* modelKiski = createModel<KiskiModule, KiskiWidget>("Kiski");
