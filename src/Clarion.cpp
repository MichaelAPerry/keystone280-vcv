// VCV wrapper for the CP3A-M mixer. DSP lives in engine/modules/Mixer3.hpp.
#include "common.hpp"
#include "layout/Clarion.hpp"
#include "modules/Mixer3.hpp"

namespace L = layout_Clarion;
using k280::Mixer3;

struct ClarionModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { GAIN1, GAIN2, GAIN3, GAIN4, MASTER, PARAMS_LEN };
    enum InputId { IN1, IN2, IN3, IN4, MA_IN, MB_IN, INPUTS_LEN };
    enum OutputId {
        MA_OUT2, MA_OUT3, MA_OUT4, MB_OUT2, MB_OUT3, MB_OUT4, OUTP1, OUTP2, OUTN1, OUTN2,
        OUTPUTS_LEN
    };
    enum LightId { LIGHTS_LEN };

    Mixer3 mixer;

    // Channel gain shown as a multiplier through the engine table.
    struct GainQuantity : k280vcv::ClockQuantity {
        static float lookup(float knob) {
            static const Mixer3 table;
            const auto& t = table.cal.gainTable;
            if (knob <= t.front().knob) return t.front().value;
            for (size_t i = 1; i < t.size(); i++) {
                if (knob <= t[i].knob) {
                    float f = (knob - t[i - 1].knob) / (t[i].knob - t[i - 1].knob);
                    return t[i - 1].value + f * (t[i].value - t[i - 1].value);
                }
            }
            return t.back().value;
        }
        float getDisplayValue() override { return lookup(getValue()) * 1.f; }
        void setDisplayValue(float d) override {
            if (!std::isfinite(d)) return;
            float lo = 0.f, hi = 1.f;  // table is monotonic in the knob
            for (int i = 0; i < 30; i++) {
                float mid = 0.5f * (lo + hi);
                (lookup(mid) * 1.f < d ? lo : hi) = mid;
            }
            setImmediateValue(0.5f * (lo + hi));
        }
        std::string getUnit() override { return " x"; }
    };

    // Master gain shown as a multiplier through the engine table.
    struct MasterQuantity : k280vcv::ClockQuantity {
        static float lookup(float knob) {
            static const Mixer3 table;
            const auto& t = table.cal.masterTable;
            if (knob <= t.front().knob) return t.front().value;
            for (size_t i = 1; i < t.size(); i++) {
                if (knob <= t[i].knob) {
                    float f = (knob - t[i - 1].knob) / (t[i].knob - t[i - 1].knob);
                    return t[i - 1].value + f * (t[i].value - t[i - 1].value);
                }
            }
            return t.back().value;
        }
        float getDisplayValue() override { return lookup(getValue()) * 1.f; }
        void setDisplayValue(float d) override {
            if (!std::isfinite(d)) return;
            float lo = 0.f, hi = 1.f;  // table is monotonic in the knob
            for (int i = 0; i < 30; i++) {
                float mid = 0.5f * (lo + hi);
                (lookup(mid) * 1.f < d ? lo : hi) = mid;
            }
            setImmediateValue(0.5f * (lo + hi));
        }
        std::string getUnit() override { return " x"; }
    };

    ClarionModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 4; i++) configParam<GainQuantity>(GAIN1 + i, 0.f, 1.f, 0.f, string::f("Channel %d gain", i + 1));
        configParam<MasterQuantity>(MASTER, 0.f, 1.f, 0.5f, "Master gain");

        for (int i = 0; i < 4; i++) configInput(IN1 + i, string::f("Channel %d in", i + 1));
        configInput(MA_IN, "Multiple A in");
        configInput(MB_IN, "Multiple B in");
        for (int i = 0; i < 3; i++) configOutput(MA_OUT2 + i, string::f("Multiple A out %d (copy of A in)", i + 2));
        for (int i = 0; i < 3; i++) configOutput(MB_OUT2 + i, string::f("Multiple B out %d (copy of B in)", i + 2));
        configOutput(OUTP1, "Mix out + 1");
        configOutput(OUTP2, "Mix out + 2");
        configOutput(OUTN1, "Mix out - 1 (inverted)");
        configOutput(OUTN2, "Mix out - 2 (inverted)");
    }

    void process(const ProcessArgs& args) override {
        Mixer3::Params p;
        for (int i = 0; i < 4; i++) p.gain[i] = params[GAIN1 + i].getValue();
        p.master = params[MASTER].getValue();

        Mixer3::Inputs in;
        for (int i = 0; i < 4; i++) in.in[i] = k280vcv::inputVoltage(inputs[IN1 + i]);
        in.maIn = k280vcv::inputVoltage(inputs[MA_IN]);
        in.mbIn = k280vcv::inputVoltage(inputs[MB_IN]);

        Mixer3::Outputs o = mixer.process(args.sampleRate, p, in);
        for (int i = 0; i < 3; i++) {
            outputs[MA_OUT2 + i].setVoltage(o.ma);
            outputs[MB_OUT2 + i].setVoltage(o.mb);
        }
        outputs[OUTP1].setVoltage(o.outP);
        outputs[OUTP2].setVoltage(o.outP);
        outputs[OUTN1].setVoltage(o.outN);
        outputs[OUTN2].setVoltage(o.outN);
    }
};

struct ClarionWidget : ModuleWidget {
    ClarionWidget(ClarionModule* module) {
        using M = ClarionModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Clarion.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::GAIN1_X, L::GAIN1_Y)), module, M::GAIN1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::GAIN2_X, L::GAIN2_Y)), module, M::GAIN2));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::GAIN3_X, L::GAIN3_Y)), module, M::GAIN3));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::GAIN4_X, L::GAIN4_Y)), module, M::GAIN4));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::MASTER_X, L::MASTER_Y)), module, M::MASTER));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN1_X, L::IN1_Y)), module, M::IN1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN2_X, L::IN2_Y)), module, M::IN2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN3_X, L::IN3_Y)), module, M::IN3));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN4_X, L::IN4_Y)), module, M::IN4));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::MA_IN_X, L::MA_IN_Y)), module, M::MA_IN));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::MB_IN_X, L::MB_IN_Y)), module, M::MB_IN));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MA_OUT2_X, L::MA_OUT2_Y)), module, M::MA_OUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MA_OUT3_X, L::MA_OUT3_Y)), module, M::MA_OUT3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MA_OUT4_X, L::MA_OUT4_Y)), module, M::MA_OUT4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MB_OUT2_X, L::MB_OUT2_Y)), module, M::MB_OUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MB_OUT3_X, L::MB_OUT3_Y)), module, M::MB_OUT3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::MB_OUT4_X, L::MB_OUT4_Y)), module, M::MB_OUT4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUTP1_X, L::OUTP1_Y)), module, M::OUTP1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUTP2_X, L::OUTP2_Y)), module, M::OUTP2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUTN1_X, L::OUTN1_Y)), module, M::OUTN1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUTN2_X, L::OUTN2_Y)), module, M::OUTN2));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Mixer3::kConfidence, "Master gain was measured. Channel gain taper is modeled.");
    }
};

Model* modelClarion = createModel<ClarionModule, ClarionWidget>("Clarion");
