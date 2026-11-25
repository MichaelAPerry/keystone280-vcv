// VCV wrapper for the 961 interface. DSP lives in engine/modules/Interface961.hpp.
#include "common.hpp"
#include "layout/Conestoga.hpp"
#include "modules/Interface961.hpp"

namespace L = layout_Conestoga;
using k280::Interface961;

struct ConestogaModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { SENS, TIME1, TIME2, PARAMS_LEN };
    enum InputId {
        AUD_IN, S_IN1, A11, A12, A13, B11, B12, B13, S_IN2, A21, A22, A23, B21, B22, B23, A14, A15, A16, B14, B15, B16, A24, A25, A26, B24, B25, B26, INPUTS_LEN
    };
    enum OutputId { AVT1, AVT2, SV1a, SV1b, SOUT1, SV2a, SV2b, SOUT2, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Interface961 iface;

    // Switch-on time shown as ms below one second, seconds above, using the engine's taper.
    struct TimeQuantity : k280vcv::ClockQuantity {
        std::string getDisplayValueString() override {
            float s = Interface961().switchOnSeconds(getValue());
            return s < 1.f ? string::f("%.0f ms", s * 1000.f) : string::f("%.2f s", s);
        }
        std::string getUnit() override { return ""; }
        float getDisplayValue() override { return Interface961().switchOnSeconds(getValue()); }
        // The taper is monotonic: invert it by bisection.
        void setDisplayValue(float d) override {
            if (!std::isfinite(d)) return;
            Interface961 e;
            float lo = 0.f, hi = 1.f;
            for (int i = 0; i < 24; i++) {
                float mid = 0.5f * (lo + hi);
                (e.switchOnSeconds(mid) < d ? lo : hi) = mid;
            }
            setImmediateValue(0.5f * (lo + hi));
        }
    };

    ConestogaModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<k280vcv::ClockQuantity>(SENS, 0.f, 1.f, 0.5f, "Audio sensitivity", "", 0.f, 10.f);
        configParam<TimeQuantity>(TIME1, 0.f, 1.f, 0.f, "Bank 1 switch-on time");
        configParam<TimeQuantity>(TIME2, 0.f, 1.f, 0.f, "Bank 2 switch-on time");

        configInput(AUD_IN, "Audio in (AC coupled)");
        configInput(S_IN1, "Bank 1 S-trig in (active low)");
        configInput(A11, "Bank 1 V-trig A 1 (DC coupled, on at 1.5 V)");
        configInput(A12, "Bank 1 V-trig A 2 (DC coupled, on at 1.5 V)");
        configInput(A13, "Bank 1 V-trig A 3 (DC coupled, on at 1.5 V)");
        configInput(B11, "Bank 1 V-trig B 1 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(B12, "Bank 1 V-trig B 2 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(B13, "Bank 1 V-trig B 3 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(S_IN2, "Bank 2 S-trig in (active low)");
        configInput(A21, "Bank 2 V-trig A 1 (DC coupled, on at 1.5 V)");
        configInput(A22, "Bank 2 V-trig A 2 (DC coupled, on at 1.5 V)");
        configInput(A23, "Bank 2 V-trig A 3 (DC coupled, on at 1.5 V)");
        configInput(B21, "Bank 2 V-trig B 1 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(B22, "Bank 2 V-trig B 2 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(B23, "Bank 2 V-trig B 3 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(A14, "Bank 1 V-trig A 4 (DC coupled, on at 1.5 V)");
        configInput(A15, "Bank 1 V-trig A 5 (DC coupled, on at 1.5 V)");
        configInput(A16, "Bank 1 V-trig A 6 (DC coupled, on at 1.5 V)");
        configInput(B14, "Bank 1 V-trig B 4 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(B15, "Bank 1 V-trig B 5 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(B16, "Bank 1 V-trig B 6 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(A24, "Bank 2 V-trig A 4 (DC coupled, on at 1.5 V)");
        configInput(A25, "Bank 2 V-trig A 5 (DC coupled, on at 1.5 V)");
        configInput(A26, "Bank 2 V-trig A 6 (DC coupled, on at 1.5 V)");
        configInput(B24, "Bank 2 V-trig B 4 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(B25, "Bank 2 V-trig B 5 (AC coupled, held by switch-on time, on at 1.5 V)");
        configInput(B26, "Bank 2 V-trig B 6 (AC coupled, held by switch-on time, on at 1.5 V)");
        configOutput(AVT1, "Audio to V-trig out 1 (+5 V)");
        configOutput(AVT2, "Audio to V-trig out 2 (+5 V)");
        configOutput(SV1a, "Bank 1 S-to-V out (+5 V while S-trig is low)");
        configOutput(SV1b, "Bank 1 S-to-V out (+5 V while S-trig is low)");
        configOutput(SOUT1, "Bank 1 S-trig out (active low, idle +12 V)");
        configOutput(SV2a, "Bank 2 S-to-V out (+5 V while S-trig is low)");
        configOutput(SV2b, "Bank 2 S-to-V out (+5 V while S-trig is low)");
        configOutput(SOUT2, "Bank 2 S-trig out (active low, idle +12 V)");
    }

    void process(const ProcessArgs& args) override {
        Interface961::Params p;
        p.sens = params[SENS].getValue();
        p.time[0] = params[TIME1].getValue();
        p.time[1] = params[TIME2].getValue();

        Interface961::Inputs in;
        in.audio = k280vcv::inputVoltage(inputs[AUD_IN]);
        in.audioConnected = inputs[AUD_IN].isConnected();
        const int sIn[2] = {S_IN1, S_IN2};
        // A and B jack ids per bank, jack 1..6.
        const int a[2][6] = {{A11, A12, A13, A14, A15, A16}, {A21, A22, A23, A24, A25, A26}};
        const int b[2][6] = {{B11, B12, B13, B14, B15, B16}, {B21, B22, B23, B24, B25, B26}};
        for (int k = 0; k < 2; k++) {
            auto& bank = in.bank[k];
            bank.sIn = k280vcv::inputVoltage(inputs[sIn[k]]);
            bank.sInConnected = inputs[sIn[k]].isConnected();
            for (int j = 0; j < Interface961::kJacksPerColumn; j++) {
                bank.a[j] = k280vcv::inputVoltage(inputs[a[k][j]]);
                bank.aConnected[j] = inputs[a[k][j]].isConnected();
                bank.b[j] = k280vcv::inputVoltage(inputs[b[k][j]]);
                bank.bConnected[j] = inputs[b[k][j]].isConnected();
            }
        }

        Interface961::Outputs o = iface.process(args.sampleRate, p, in);
        outputs[AVT1].setVoltage(o.avt);
        outputs[AVT2].setVoltage(o.avt);
        outputs[SV1a].setVoltage(o.bank[0].sv);
        outputs[SV1b].setVoltage(o.bank[0].sv);
        outputs[SOUT1].setVoltage(o.bank[0].sOut);
        outputs[SV2a].setVoltage(o.bank[1].sv);
        outputs[SV2b].setVoltage(o.bank[1].sv);
        outputs[SOUT2].setVoltage(o.bank[1].sOut);
    }
};

struct ConestogaWidget : ModuleWidget {
    ConestogaWidget(ConestogaModule* module) {
        using M = ConestogaModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Conestoga.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::SENS_X, L::SENS_Y)), module, M::SENS));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::TIME1_X, L::TIME1_Y)), module, M::TIME1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::TIME2_X, L::TIME2_Y)), module, M::TIME2));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::AUD_IN_X, L::AUD_IN_Y)), module, M::AUD_IN));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::S_IN1_X, L::S_IN1_Y)), module, M::S_IN1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A11_X, L::A11_Y)), module, M::A11));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A12_X, L::A12_Y)), module, M::A12));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A13_X, L::A13_Y)), module, M::A13));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B11_X, L::B11_Y)), module, M::B11));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B12_X, L::B12_Y)), module, M::B12));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B13_X, L::B13_Y)), module, M::B13));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::S_IN2_X, L::S_IN2_Y)), module, M::S_IN2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A21_X, L::A21_Y)), module, M::A21));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A22_X, L::A22_Y)), module, M::A22));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A23_X, L::A23_Y)), module, M::A23));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B21_X, L::B21_Y)), module, M::B21));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B22_X, L::B22_Y)), module, M::B22));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B23_X, L::B23_Y)), module, M::B23));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A14_X, L::A14_Y)), module, M::A14));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A15_X, L::A15_Y)), module, M::A15));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A16_X, L::A16_Y)), module, M::A16));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B14_X, L::B14_Y)), module, M::B14));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B15_X, L::B15_Y)), module, M::B15));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B16_X, L::B16_Y)), module, M::B16));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A24_X, L::A24_Y)), module, M::A24));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A25_X, L::A25_Y)), module, M::A25));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::A26_X, L::A26_Y)), module, M::A26));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B24_X, L::B24_Y)), module, M::B24));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B25_X, L::B25_Y)), module, M::B25));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::B26_X, L::B26_Y)), module, M::B26));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::AVT1_X, L::AVT1_Y)), module, M::AVT1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::AVT2_X, L::AVT2_Y)), module, M::AVT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SV1a_X, L::SV1a_Y)), module, M::SV1a));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SV1b_X, L::SV1b_Y)), module, M::SV1b));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT1_X, L::SOUT1_Y)), module, M::SOUT1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SV2a_X, L::SV2a_Y)), module, M::SV2a));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SV2b_X, L::SV2b_Y)), module, M::SV2b));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT2_X, L::SOUT2_Y)), module, M::SOUT2));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Interface961::kConfidence,
                                  "Bank 1 switch-on time measured on the bench. Audio detector fitted to noise tests.");
    }
};

Model* modelConestoga = createModel<ConestogaModule, ConestogaWidget>("Conestoga");
