// VCV wrapper for the Behringer Abacus. DSP lives in engine/modules/Abacus.hpp.
// Port and param ids are the layout ids (the book's jack keys, manual/tools/rack.py); the four time knobs
// carry a _KNOB suffix because RISE1/FALL1/RISE4/FALL4 are also jack keys. Append only: patch files
// store ids by number.
#include <atomic>

#include "common.hpp"
#include "layout/Loyalsock.hpp"
#include "modules/Abacus.hpp"

namespace L = layout_Loyalsock;
using k280::Abacus;

struct LoyalsockModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId {
        RISE1_KNOB, FALL1_KNOB, RESPONSE1, CYCLE_BTN1, RISE4_KNOB, FALL4_KNOB, RESPONSE4, CYCLE_BTN4, ATT1, ATT4, ATT2, ATT3,
        PARAMS_LEN
    };
    enum InputId { IN1, TRIG1, IN2, IN3, TRIG4, IN4, RISE1, BOTH1, FALL1, CYCLE1, RISE4, BOTH4, FALL4, CYCLE4, INPUTS_LEN };
    enum OutputId { OUT1, OUT2, OUT3, OUT4, EOR, UNITY1, OR, SUM, INV, UNITY4, EOC, OUTPUTS_LEN };
    enum LightId { CYCLE1_LIGHT, CYCLE4_LIGHT, EOR_LIGHT, EOC_LIGHT, LIGHTS_LEN };

    Abacus abacus;
    // Set from the UI thread (context menu), read on the audio thread.
    std::atomic<bool> patchedOutLeavesBus{true};

    // RISE / FALL knob shown as a time, using the current RESPONSE setting.
    struct TimeQuantity : ParamQuantity {
        int responseId = 0;
        bool fall = false;
        float seconds(float knob) const {
            float r = module ? module->params[responseId].getValue() : .5f;
            return Abacus().timeSeconds(knob, r, fall);
        }
        static std::string format(float s) {
            return s < 1.f ? string::f("%.1f ms", s * 1000.f) : string::f("%.2f s", s);
        }
        std::string getDisplayValueString() override { return format(seconds(getValue())); }
        std::string getUnit() override { return ""; }
        float getDisplayValue() override { return seconds(getValue()); }
        // The time taper is monotonic in the knob: invert it by bisection.
        void setDisplayValue(float d) override {
            if (!std::isfinite(d)) return;
            float lo = 0.f, hi = 1.f;
            for (int i = 0; i < 24; i++) {
                float mid = 0.5f * (lo + hi);
                (seconds(mid) < d ? lo : hi) = mid;
            }
            setImmediateValue(0.5f * (lo + hi));
        }
    };

    struct ResponseQuantity : ParamQuantity {
        std::string getDisplayValueString() override {
            float v = getValue();
            if (std::fabs(v - .5f) < .02f) return "lin";
            return string::f("%s %.0f %%", v < .5f ? "log" : "exp", std::fabs(2.f * v - 1.f) * 100.f);
        }
        std::string getUnit() override { return ""; }
    };

    LoyalsockModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        const int riseId[2] = {RISE1_KNOB, RISE4_KNOB}, fallId[2] = {FALL1_KNOB, FALL4_KNOB};
        const int respId[2] = {RESPONSE1, RESPONSE4}, btnId[2] = {CYCLE_BTN1, CYCLE_BTN4};
        const int chNum[2] = {1, 4};
        for (int c = 0; c < 2; c++) {
            auto* r = configParam<TimeQuantity>(riseId[c], 0.f, 1.f, 0.f, string::f("CH%d rise time", chNum[c]));
            r->responseId = respId[c];
            r->fall = false;
            auto* f = configParam<TimeQuantity>(fallId[c], 0.f, 1.f, 0.f, string::f("CH%d fall time", chNum[c]));
            f->responseId = respId[c];
            f->fall = true;
            configParam<ResponseQuantity>(respId[c], 0.f, 1.f, .5f, string::f("CH%d response (log ... lin ... exp)", chNum[c]));
            configButton(btnId[c], string::f("CH%d cycle (toggles)", chNum[c]));
        }
        const int attId[4] = {ATT1, ATT2, ATT3, ATT4};
        for (int i = 0; i < 4; i++)
            configParam(attId[i], 0.f, 1.f, .5f, string::f("CH%d attenuverter", i + 1), " %", 0.f, 200.f, -100.f);

        configInput(IN1, "CH1 signal in (normalled +10 V)");
        // Abacus QSG QSG_BE_0720-ACO_ABACUS_WW_af3638.txt, pp. 4, 20.
        // Bench test 14 observed intermittent firing near +0.8 V.
        configInput(TRIG1, "CH1 trigger (rack: reliable fast pulses at ~+1.3–1.4 V)");
        configInput(IN2, "CH2 in (normalled to internal voltage)");
        configInput(IN3, "CH3 in (normalled to internal voltage)");
        configInput(TRIG4, "CH4 trigger (CH1 rack test: reliable fast pulses at ~+1.3–1.4 V)");
        configInput(IN4, "CH4 signal in (normalled +10 V)");
        configInput(RISE1, "CH1 rise CV (+/-8 V)");
        configInput(BOTH1, "CH1 both CV (+/-8 V, 1 V per time octave)");
        configInput(FALL1, "CH1 fall CV (+/-8 V)");
        configInput(CYCLE1, "CH1 cycle (+2.5 V threshold)");
        configInput(RISE4, "CH4 rise CV (+/-8 V)");
        configInput(BOTH4, "CH4 both CV (+/-8 V, 1 V per time octave)");
        configInput(FALL4, "CH4 fall CV (+/-8 V)");
        configInput(CYCLE4, "CH4 cycle (+2.5 V threshold)");
        for (int i = 0; i < 4; i++) configOutput(OUT1 + i, string::f("CH%d out", i + 1));
        configOutput(EOR, "CH1 end of rise (+9 V from the top of the rise to the end of the fall)");
        configOutput(UNITY1, "CH1 unity (0 to +10 V, before the attenuverter)");
        configOutput(OR, "OR of the channel outputs");
        configOutput(SUM, "SUM of the channel outputs (+/-10 V)");
        configOutput(INV, "Inverted SUM");
        configOutput(UNITY4, "CH4 unity (0 to +10 V, before the attenuverter)");
        configOutput(EOC, "CH4 end of cycle (+9 V)");
        configLight(CYCLE1_LIGHT, "CH1 cycling");
        configLight(CYCLE4_LIGHT, "CH4 cycling");
        configLight(EOR_LIGHT, "End of rise");
        configLight(EOC_LIGHT, "End of cycle");
    }

    void process(const ProcessArgs& args) override {
        Abacus::Params p;
        p.rise1 = params[RISE1_KNOB].getValue();
        p.fall1 = params[FALL1_KNOB].getValue();
        p.response1 = params[RESPONSE1].getValue();
        p.rise4 = params[RISE4_KNOB].getValue();
        p.fall4 = params[FALL4_KNOB].getValue();
        p.response4 = params[RESPONSE4].getValue();
        p.cycleBtn1 = params[CYCLE_BTN1].getValue() > 0.5f;
        p.cycleBtn4 = params[CYCLE_BTN4].getValue() > 0.5f;
        p.att1 = params[ATT1].getValue();
        p.att2 = params[ATT2].getValue();
        p.att3 = params[ATT3].getValue();
        p.att4 = params[ATT4].getValue();
        p.patchedOutLeavesBus = patchedOutLeavesBus.load(std::memory_order_relaxed);

        Abacus::Inputs in;
        in.in1 = inputs[IN1].getVoltage();    in.in1Connected = inputs[IN1].isConnected();
        in.in2 = inputs[IN2].getVoltage();    in.in2Connected = inputs[IN2].isConnected();
        in.in3 = inputs[IN3].getVoltage();    in.in3Connected = inputs[IN3].isConnected();
        in.in4 = inputs[IN4].getVoltage();    in.in4Connected = inputs[IN4].isConnected();
        in.trig1 = inputs[TRIG1].getVoltage(); in.trig1Connected = inputs[TRIG1].isConnected();
        in.trig4 = inputs[TRIG4].getVoltage(); in.trig4Connected = inputs[TRIG4].isConnected();
        in.rise1 = inputs[RISE1].getVoltage(); in.rise1Connected = inputs[RISE1].isConnected();
        in.both1 = inputs[BOTH1].getVoltage(); in.both1Connected = inputs[BOTH1].isConnected();
        in.fall1 = inputs[FALL1].getVoltage(); in.fall1Connected = inputs[FALL1].isConnected();
        in.cycle1 = inputs[CYCLE1].getVoltage(); in.cycle1Connected = inputs[CYCLE1].isConnected();
        in.rise4 = inputs[RISE4].getVoltage(); in.rise4Connected = inputs[RISE4].isConnected();
        in.both4 = inputs[BOTH4].getVoltage(); in.both4Connected = inputs[BOTH4].isConnected();
        in.fall4 = inputs[FALL4].getVoltage(); in.fall4Connected = inputs[FALL4].isConnected();
        in.cycle4 = inputs[CYCLE4].getVoltage(); in.cycle4Connected = inputs[CYCLE4].isConnected();
        for (int i = 0; i < 4; i++) in.outConnected[i] = outputs[OUT1 + i].isConnected();

        Abacus::Outputs o = abacus.process(args.sampleRate, p, in);
        for (int i = 0; i < 4; i++) outputs[OUT1 + i].setVoltage(o.out[i]);
        outputs[EOR].setVoltage(o.eor);
        outputs[UNITY1].setVoltage(o.unity1);
        outputs[OR].setVoltage(o.orOut);
        outputs[SUM].setVoltage(o.sum);
        outputs[INV].setVoltage(o.inv);
        outputs[UNITY4].setVoltage(o.unity4);
        outputs[EOC].setVoltage(o.eoc);

        lights[CYCLE1_LIGHT].setBrightness(abacus.cycling(1) ? 1.f : 0.f);
        lights[CYCLE4_LIGHT].setBrightness(abacus.cycling(4) ? 1.f : 0.f);
        lights[EOR_LIGHT].setBrightness(o.eor > 0.f ? 1.f : 0.f);
        lights[EOC_LIGHT].setBrightness(o.eoc > 0.f ? 1.f : 0.f);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "patchedOutLeavesBus", json_boolean(patchedOutLeavesBus));
        json_object_set_new(root, "cycle1", json_boolean(abacus.cycleButtonOn(1)));
        json_object_set_new(root, "cycle4", json_boolean(abacus.cycleButtonOn(4)));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* b = json_object_get(root, "patchedOutLeavesBus");
        patchedOutLeavesBus = json_is_boolean(b) ? json_is_true(b) : true;  // missing key: Maths behaviour
        abacus.restoreCycleButton(1, json_is_true(json_object_get(root, "cycle1")));  // missing key: off
        abacus.restoreCycleButton(4, json_is_true(json_object_get(root, "cycle4")));
    }
};

struct LoyalsockWidget : ModuleWidget {
    LoyalsockWidget(LoyalsockModule* module) {
        using M = LoyalsockModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Loyalsock.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::RISE1_KNOB_X, L::RISE1_KNOB_Y)), module, M::RISE1_KNOB));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::FALL1_KNOB_X, L::FALL1_KNOB_Y)), module, M::FALL1_KNOB));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::RESPONSE1_X, L::RESPONSE1_Y)), module, M::RESPONSE1));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::CYCLE_BTN1_X, L::CYCLE_BTN1_Y)), module, M::CYCLE_BTN1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::RISE4_KNOB_X, L::RISE4_KNOB_Y)), module, M::RISE4_KNOB));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::FALL4_KNOB_X, L::FALL4_KNOB_Y)), module, M::FALL4_KNOB));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::RESPONSE4_X, L::RESPONSE4_Y)), module, M::RESPONSE4));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::CYCLE_BTN4_X, L::CYCLE_BTN4_Y)), module, M::CYCLE_BTN4));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ATT1_X, L::ATT1_Y)), module, M::ATT1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ATT4_X, L::ATT4_Y)), module, M::ATT4));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ATT2_X, L::ATT2_Y)), module, M::ATT2));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ATT3_X, L::ATT3_Y)), module, M::ATT3));

        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(L::CYCLE1_LIGHT_X, L::CYCLE1_LIGHT_Y)), module, M::CYCLE1_LIGHT));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(L::CYCLE4_LIGHT_X, L::CYCLE4_LIGHT_Y)), module, M::CYCLE4_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::EOR_LIGHT_X, L::EOR_LIGHT_Y)), module, M::EOR_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::EOC_LIGHT_X, L::EOC_LIGHT_Y)), module, M::EOC_LIGHT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN1_X, L::IN1_Y)), module, M::IN1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::TRIG1_X, L::TRIG1_Y)), module, M::TRIG1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN2_X, L::IN2_Y)), module, M::IN2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN3_X, L::IN3_Y)), module, M::IN3));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::TRIG4_X, L::TRIG4_Y)), module, M::TRIG4));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN4_X, L::IN4_Y)), module, M::IN4));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::RISE1_X, L::RISE1_Y)), module, M::RISE1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::BOTH1_X, L::BOTH1_Y)), module, M::BOTH1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::FALL1_X, L::FALL1_Y)), module, M::FALL1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CYCLE1_X, L::CYCLE1_Y)), module, M::CYCLE1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::RISE4_X, L::RISE4_Y)), module, M::RISE4));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::BOTH4_X, L::BOTH4_Y)), module, M::BOTH4));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::FALL4_X, L::FALL4_Y)), module, M::FALL4));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CYCLE4_X, L::CYCLE4_Y)), module, M::CYCLE4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT1_X, L::OUT1_Y)), module, M::OUT1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT2_X, L::OUT2_Y)), module, M::OUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT3_X, L::OUT3_Y)), module, M::OUT3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT4_X, L::OUT4_Y)), module, M::OUT4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::EOR_X, L::EOR_Y)), module, M::EOR));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::UNITY1_X, L::UNITY1_Y)), module, M::UNITY1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OR_X, L::OR_Y)), module, M::OR));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SUM_X, L::SUM_Y)), module, M::SUM));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::INV_X, L::INV_Y)), module, M::INV));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::UNITY4_X, L::UNITY4_Y)), module, M::UNITY4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::EOC_X, L::EOC_Y)), module, M::EOC));
    }

    void appendContextMenu(Menu* menu) override {
        auto* m = getModule<LoyalsockModule>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createBoolMenuItem(
            "Exclude patched outputs from SUM and OR", "",
            [=]() { return m->patchedOutLeavesBus.load(); },
            [=](bool v) { m->patchedOutLeavesBus = v; }));
        k280vcv::appendConfidence(menu, Abacus::kConfidence,
                                  "Cycle timing, trigger threshold and rest voltages were checked on the bench.");
    }
};

Model* modelLoyalsock = createModel<LoyalsockModule, LoyalsockWidget>("Loyalsock");
