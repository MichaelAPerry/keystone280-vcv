// VCV wrapper for the 962 sequential switch. DSP lives in engine/modules/Seq962.hpp.
#include "common.hpp"
#include "layout/Tioga.hpp"
#include "modules/Seq962.hpp"

namespace L = layout_Tioga;
using k280::Seq962;

struct TiogaModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { BTN1, BTN2, BTN3, PARAMS_LEN };
    enum InputId { TIN1, TIN2, TIN3, SHIFT, SIN1, SIN2, SIN3, INPUTS_LEN };
    enum OutputId { TOUT1, TOUT2, TOUT3, OUT1, OUT2, OUTPUTS_LEN };
    enum LightId { LIGHT1, LIGHT2, LIGHT3, LIGHTS_LEN };

    Seq962 seq;

    TiogaModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        for (int i = 0; i < 3; i++) configButton(BTN1 + i, string::f("Select stage %d", i + 1));

        for (int i = 0; i < 3; i++) configInput(TIN1 + i, string::f("Stage %d trigger in (select stage, +3.5 V threshold)", i + 1));
        configInput(SHIFT, "Shift (advance one stage, +1.5 V threshold)");
        for (int i = 0; i < 3; i++) configInput(SIN1 + i, string::f("Signal in %d (DC coupled)", i + 1));

        for (int i = 0; i < 3; i++) configOutput(TOUT1 + i, string::f("Stage %d trigger out (+5 V while active)", i + 1));
        configOutput(OUT1, "Signal out (active stage's input)");
        configOutput(OUT2, "Signal out (parallel jack)");

        for (int i = 0; i < 3; i++) configLight(LIGHT1 + i, string::f("Stage %d active", i + 1));
    }

    void onReset() override { seq.reset(); }

    void process(const ProcessArgs& args) override {
        Seq962::Inputs in;
        in.shiftV = inputs[SHIFT].getVoltage();
        in.shiftConnected = inputs[SHIFT].isConnected();
        for (int i = 0; i < 3; i++) {
            in.tin[i] = inputs[TIN1 + i].getVoltage();
            in.tinConnected[i] = inputs[TIN1 + i].isConnected();
            in.sin[i] = inputs[SIN1 + i].getVoltage();
            in.sinConnected[i] = inputs[SIN1 + i].isConnected();
            in.btn[i] = params[BTN1 + i].getValue() > 0.5f;
        }

        Seq962::Outputs o = seq.process(args.sampleRate, in);
        outputs[OUT1].setVoltage(o.out);
        outputs[OUT2].setVoltage(o.out);
        for (int i = 0; i < 3; i++) {
            outputs[TOUT1 + i].setVoltage(o.tout[i]);
            lights[LIGHT1 + i].setBrightness(seq.stage() == i + 1 ? 1.f : 0.f);
        }
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "stage", json_integer(seq.saveStage()));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* s = json_object_get(root, "stage");
        if (json_is_integer(s)) seq.restoreStage(int(json_integer_value(s)));
    }
};

struct TiogaWidget : ModuleWidget {
    TiogaWidget(TiogaModule* module) {
        using M = TiogaModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Tioga.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::BTN1_X, L::BTN1_Y)), module, M::BTN1));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::BTN2_X, L::BTN2_Y)), module, M::BTN2));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::BTN3_X, L::BTN3_Y)), module, M::BTN3));

        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::LIGHT1_X, L::LIGHT1_Y)), module, M::LIGHT1));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::LIGHT2_X, L::LIGHT2_Y)), module, M::LIGHT2));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::LIGHT3_X, L::LIGHT3_Y)), module, M::LIGHT3));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::TIN1_X, L::TIN1_Y)), module, M::TIN1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::TIN2_X, L::TIN2_Y)), module, M::TIN2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::TIN3_X, L::TIN3_Y)), module, M::TIN3));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SHIFT_X, L::SHIFT_Y)), module, M::SHIFT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN1_X, L::SIN1_Y)), module, M::SIN1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN2_X, L::SIN2_Y)), module, M::SIN2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN3_X, L::SIN3_Y)), module, M::SIN3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::TOUT1_X, L::TOUT1_Y)), module, M::TOUT1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::TOUT2_X, L::TOUT2_Y)), module, M::TOUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::TOUT3_X, L::TOUT3_Y)), module, M::TOUT3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT1_X, L::OUT1_Y)), module, M::OUT1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT2_X, L::OUT2_Y)), module, M::OUT2));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Seq962::kConfidence, "Stage advance with an unplugged third input was checked on the bench.");
    }
};

Model* modelTioga = createModel<TiogaModule, TiogaWidget>("Tioga");
