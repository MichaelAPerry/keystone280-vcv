// VCV wrapper for the 960 sequential controller. DSP lives in engine/modules/Seq960.hpp.
#include <atomic>

#include "common.hpp"
#include "layout/Juniata.hpp"
#include "modules/Seq960.hpp"

namespace L = layout_Juniata;
using k280::Seq960;

namespace {
// Horizontal two-position rocker: value 0 = left, 1 = right (frames of the stock horizontal switch).
struct TimingSwitch : app::SvgSwitch {
    TimingSwitch() {
        shadow->opacity = 0.f;
        addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThreeHorizontal_0.svg")));
        addFrame(Svg::load(asset::system("res/ComponentLibrary/CKSSThreeHorizontal_2.svg")));
    }
};
}  // namespace

struct JuniataModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId {
        FREQ_RANGE, VERNIER, OSC_ON_BTN, OSC_OFF_BTN, KNOB_A1, KNOB_A2, KNOB_A3, KNOB_A4, KNOB_A5, KNOB_A6,
        KNOB_A7, KNOB_A8, KNOB_B1, KNOB_B2, KNOB_B3, KNOB_B4, KNOB_B5, KNOB_B6, KNOB_B7, KNOB_B8, KNOB_C1,
        KNOB_C2, KNOB_C3, KNOB_C4, KNOB_C5, KNOB_C6, KNOB_C7, KNOB_C8, MODE1, MODE2, MODE3, MODE4, MODE5,
        MODE6, MODE7, MODE8, MODE9, SET1, SET2, SET3, SET4, SET5, SET6, SET7, SET8, SET9, RANGE_A, RANGE_B,
        RANGE_C, TIMING, SHIFT_BTN,
        PARAMS_LEN
    };
    enum InputId { OSC_ON, OSC_OFF, OSC_CV, SIN1, SIN2, SIN3, SIN4, SIN5, SIN6, SIN7, SIN8, SIN9, SHIFT, INPUTS_LEN };
    enum OutputId {
        OSC_OUT, SOUT1, SOUT2, SOUT3, SOUT4, SOUT5, SOUT6, SOUT7, SOUT8, SOUT9, ROWA1, ROWA2, ROWB1, ROWB2,
        ROWC1, ROWC2,
        OUTPUTS_LEN
    };
    enum LightId { OSC_LIGHT, STAGE_LIGHT1, STAGE_LIGHT2, STAGE_LIGHT3, STAGE_LIGHT4, STAGE_LIGHT5, STAGE_LIGHT6, STAGE_LIGHT7, STAGE_LIGHT8, STAGE_LIGHT9, LIGHTS_LEN };

    Seq960 seq;
    // Set from the UI thread (context menu), read on the audio thread.
    std::atomic<Seq960::StopMode> stopMode{Seq960::StopMode::BlockShift};

    // Row knob shown in volts, scaled by the row's current X1 / X2 / X4 range.
    struct RowQuantity : k280vcv::ClockQuantity {
        int row = 0;
        float maxVolts() const {
            static const float v[3] = {2.f, 4.f, 8.f};
            int r = module ? int(std::round(module->params[RANGE_A + row].getValue())) : 0;
            return v[math::clamp(r, 0, 2)];
        }
        float getDisplayValue() override { return getValue() * maxVolts(); }
        void setDisplayValue(float d) override {
            if (std::isfinite(d)) setImmediateValue(math::clamp(d / maxVolts(), 0.f, 1.f));
        }
        std::string getUnit() override { return " V"; }
    };

    JuniataModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configSwitch(FREQ_RANGE, 0.f, 5.f, 4.f, "Oscillator frequency range",
                     {"1 (0.04–0.5 Hz)", "2 (2.75–30 Hz)", "3 (0.17–2 Hz)", "4 (11–130 Hz)", "5 (0.7–8 Hz)", "6 (44–500 Hz)"});
        configParam<k280vcv::ClockQuantity>(VERNIER, 0.f, 1.f, 0.5f, "Frequency vernier", "", 0.f, 10.f);
        configButton(OSC_ON_BTN, "Oscillator on");
        configButton(OSC_OFF_BTN, "Oscillator off");
        for (int r = 0; r < 3; r++) {
            for (int i = 0; i < 8; i++) {
                auto* q = configParam<RowQuantity>(KNOB_A1 + r * 8 + i, 0.f, 1.f, 0.f,
                                                   string::f("Row %c voltage, stage %d", 'A' + r, i + 1));
                q->row = r;
            }
        }
        for (int i = 0; i < 8; i++)
            configSwitch(MODE1 + i, 0.f, 2.f, 1.f, string::f("Stage %d mode", i + 1), {"Skip", "Normal", "Stop"});
        configSwitch(MODE9, 0.f, 1.f, 0.f, "Stage 9 (OFF) mode", {"Skip", "Stop"});
        for (int i = 0; i < 9; i++) configButton(SET1 + i, string::f("Set stage %d", i + 1));
        for (int r = 0; r < 3; r++)
            configSwitch(RANGE_A + r, 0.f, 2.f, 0.f, string::f("Row %c range", 'A' + r),
                         {"X1 (0–2 V)", "X2 (0–4 V)", "X4 (0–8 V)"});
        configSwitch(TIMING, 0.f, 1.f, 0.f, "3rd row control of timing", {"Off", "On"});
        configButton(SHIFT_BTN, "Shift (advance one stage)");

        configInput(OSC_ON, "Oscillator on (AC coupled, +3.5 V threshold)");
        configInput(OSC_OFF, "Oscillator off (AC coupled, +3.5 V threshold)");
        configInput(OSC_CV, "Oscillator frequency CV (1 V/oct, ±2 V)");
        for (int i = 0; i < 9; i++) configInput(SIN1 + i, string::f("Stage %d in (jump to stage, +3.5 V threshold)", i + 1));
        // Behringer_System_15_35_55_Manual.txt, p. 20; rack bench tests 8 and 22.
        configInput(SHIFT, "Shift (rack advances with +0.571 V peak; ±5 V rated max)");
        configOutput(OSC_OUT, "Oscillator out (rack: ~32% high)");
        for (int i = 0; i < 9; i++) configOutput(SOUT1 + i, string::f("Stage %d out (+5 V while active)", i + 1));
        const char* rowName[3] = {"A", "B", "C"};
        for (int r = 0; r < 3; r++) {
            configOutput(ROWA1 + 2 * r, string::f("Row %s out 1", rowName[r]));
            configOutput(ROWA1 + 2 * r + 1, string::f("Row %s out 2", rowName[r]));
        }
        configLight(OSC_LIGHT, "Oscillator running");
        for (int i = 0; i < 9; i++) configLight(STAGE_LIGHT1 + i, string::f("Stage %d active", i + 1));
    }

    void onReset() override { seq.reset(); }

    void process(const ProcessArgs& args) override {
        Seq960::Params p;
        p.freqRange = int(std::round(params[FREQ_RANGE].getValue()));
        p.vernier = params[VERNIER].getValue();
        for (int r = 0; r < 3; r++) {
            for (int i = 0; i < 8; i++) p.knobs[r][i] = params[KNOB_A1 + r * 8 + i].getValue();
            p.rowRange[r] = int(std::round(params[RANGE_A + r].getValue()));
        }
        for (int i = 0; i < 8; i++) p.mode[i] = int(std::round(params[MODE1 + i].getValue()));
        p.mode9 = int(std::round(params[MODE9].getValue()));
        p.timing = params[TIMING].getValue() > 0.5f;
        p.stopMode = stopMode.load(std::memory_order_relaxed);

        Seq960::Inputs in;
        in.oscOn = params[OSC_ON_BTN].getValue() > 0.5f;
        in.oscOff = params[OSC_OFF_BTN].getValue() > 0.5f;
        in.shift = params[SHIFT_BTN].getValue() > 0.5f;
        in.oscOnV = inputs[OSC_ON].getVoltage();
        in.oscOffV = inputs[OSC_OFF].getVoltage();
        in.shiftV = inputs[SHIFT].getVoltage();
        in.oscCv = inputs[OSC_CV].getVoltage();
        in.oscOnConnected = inputs[OSC_ON].isConnected();
        in.oscOffConnected = inputs[OSC_OFF].isConnected();
        in.shiftConnected = inputs[SHIFT].isConnected();
        in.oscCvConnected = inputs[OSC_CV].isConnected();
        for (int i = 0; i < 9; i++) {
            in.set[i] = params[SET1 + i].getValue() > 0.5f;
            in.sin[i] = inputs[SIN1 + i].getVoltage();
            in.sinConnected[i] = inputs[SIN1 + i].isConnected();
        }

        Seq960::Outputs o = seq.process(args.sampleRate, p, in);
        outputs[OSC_OUT].setVoltage(o.oscOut);
        for (int i = 0; i < 9; i++) outputs[SOUT1 + i].setVoltage(o.sout[i]);
        for (int r = 0; r < 3; r++) {
            outputs[ROWA1 + 2 * r].setVoltage(o.row[r]);
            outputs[ROWA1 + 2 * r + 1].setVoltage(o.row[r]);
        }

        lights[OSC_LIGHT].setBrightness(seq.oscRunning() ? 1.f : 0.f);
        for (int i = 0; i < 9; i++) lights[STAGE_LIGHT1 + i].setBrightness(seq.stage() == i + 1 ? 1.f : 0.f);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "stopMode", json_string(stopMode == Seq960::StopMode::BlockShift ? "blockShift" : "oscOnly"));
        json_object_set_new(root, "stage", json_integer(seq.saveStage()));
        json_object_set_new(root, "oscRunning", json_boolean(seq.oscRunning()));
        return root;
    }

    void dataFromJson(json_t* root) override {
        const char* m = json_string_value(json_object_get(root, "stopMode"));  // null if absent or not a string
        stopMode = (m && std::string(m) == "oscOnly") ? Seq960::StopMode::OscOnly : Seq960::StopMode::BlockShift;
        json_t* s = json_object_get(root, "stage");
        if (json_is_integer(s)) seq.restoreStage(int(json_integer_value(s)));
        seq.restoreRunning(json_is_true(json_object_get(root, "oscRunning")));  // missing key: false
    }
};

struct JuniataWidget : ModuleWidget {
    JuniataWidget(JuniataModule* module) {
        using M = JuniataModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Juniata.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::FREQ_RANGE_X, L::FREQ_RANGE_Y)), module, M::FREQ_RANGE, L::FREQ_RANGE_MIN_DEG, L::FREQ_RANGE_MAX_DEG));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::VERNIER_X, L::VERNIER_Y)), module, M::VERNIER));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::OSC_ON_BTN_X, L::OSC_ON_BTN_Y)), module, M::OSC_ON_BTN));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::OSC_OFF_BTN_X, L::OSC_OFF_BTN_Y)), module, M::OSC_OFF_BTN));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_A1_X, L::KNOB_A1_Y)), module, M::KNOB_A1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_A2_X, L::KNOB_A2_Y)), module, M::KNOB_A2));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_A3_X, L::KNOB_A3_Y)), module, M::KNOB_A3));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_A4_X, L::KNOB_A4_Y)), module, M::KNOB_A4));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_A5_X, L::KNOB_A5_Y)), module, M::KNOB_A5));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_A6_X, L::KNOB_A6_Y)), module, M::KNOB_A6));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_A7_X, L::KNOB_A7_Y)), module, M::KNOB_A7));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_A8_X, L::KNOB_A8_Y)), module, M::KNOB_A8));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_B1_X, L::KNOB_B1_Y)), module, M::KNOB_B1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_B2_X, L::KNOB_B2_Y)), module, M::KNOB_B2));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_B3_X, L::KNOB_B3_Y)), module, M::KNOB_B3));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_B4_X, L::KNOB_B4_Y)), module, M::KNOB_B4));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_B5_X, L::KNOB_B5_Y)), module, M::KNOB_B5));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_B6_X, L::KNOB_B6_Y)), module, M::KNOB_B6));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_B7_X, L::KNOB_B7_Y)), module, M::KNOB_B7));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_B8_X, L::KNOB_B8_Y)), module, M::KNOB_B8));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_C1_X, L::KNOB_C1_Y)), module, M::KNOB_C1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_C2_X, L::KNOB_C2_Y)), module, M::KNOB_C2));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_C3_X, L::KNOB_C3_Y)), module, M::KNOB_C3));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_C4_X, L::KNOB_C4_Y)), module, M::KNOB_C4));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_C5_X, L::KNOB_C5_Y)), module, M::KNOB_C5));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_C6_X, L::KNOB_C6_Y)), module, M::KNOB_C6));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_C7_X, L::KNOB_C7_Y)), module, M::KNOB_C7));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::KNOB_C8_X, L::KNOB_C8_Y)), module, M::KNOB_C8));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE1_X, L::MODE1_Y)), module, M::MODE1, L::MODE1_MIN_DEG, L::MODE1_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE2_X, L::MODE2_Y)), module, M::MODE2, L::MODE2_MIN_DEG, L::MODE2_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE3_X, L::MODE3_Y)), module, M::MODE3, L::MODE3_MIN_DEG, L::MODE3_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE4_X, L::MODE4_Y)), module, M::MODE4, L::MODE4_MIN_DEG, L::MODE4_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE5_X, L::MODE5_Y)), module, M::MODE5, L::MODE5_MIN_DEG, L::MODE5_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE6_X, L::MODE6_Y)), module, M::MODE6, L::MODE6_MIN_DEG, L::MODE6_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE7_X, L::MODE7_Y)), module, M::MODE7, L::MODE7_MIN_DEG, L::MODE7_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE8_X, L::MODE8_Y)), module, M::MODE8, L::MODE8_MIN_DEG, L::MODE8_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::MODE9_X, L::MODE9_Y)), module, M::MODE9, L::MODE9_MIN_DEG, L::MODE9_MAX_DEG));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET1_X, L::SET1_Y)), module, M::SET1));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET2_X, L::SET2_Y)), module, M::SET2));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET3_X, L::SET3_Y)), module, M::SET3));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET4_X, L::SET4_Y)), module, M::SET4));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET5_X, L::SET5_Y)), module, M::SET5));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET6_X, L::SET6_Y)), module, M::SET6));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET7_X, L::SET7_Y)), module, M::SET7));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET8_X, L::SET8_Y)), module, M::SET8));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SET9_X, L::SET9_Y)), module, M::SET9));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::RANGE_A_X, L::RANGE_A_Y)), module, M::RANGE_A, L::RANGE_A_MIN_DEG, L::RANGE_A_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::RANGE_B_X, L::RANGE_B_Y)), module, M::RANGE_B, L::RANGE_B_MIN_DEG, L::RANGE_B_MAX_DEG));
        addParam(k280vcv::createSnapKnob<Davies1900hBlackKnob>(mm2px(Vec(L::RANGE_C_X, L::RANGE_C_Y)), module, M::RANGE_C, L::RANGE_C_MIN_DEG, L::RANGE_C_MAX_DEG));
        addParam(createParamCentered<TimingSwitch>(mm2px(Vec(L::TIMING_X, L::TIMING_Y)), module, M::TIMING));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(L::SHIFT_BTN_X, L::SHIFT_BTN_Y)), module, M::SHIFT_BTN));

        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(L::OSC_LIGHT_X, L::OSC_LIGHT_Y)), module, M::OSC_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT1_X, L::STAGE_LIGHT1_Y)), module, M::STAGE_LIGHT1));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT2_X, L::STAGE_LIGHT2_Y)), module, M::STAGE_LIGHT2));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT3_X, L::STAGE_LIGHT3_Y)), module, M::STAGE_LIGHT3));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT4_X, L::STAGE_LIGHT4_Y)), module, M::STAGE_LIGHT4));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT5_X, L::STAGE_LIGHT5_Y)), module, M::STAGE_LIGHT5));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT6_X, L::STAGE_LIGHT6_Y)), module, M::STAGE_LIGHT6));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT7_X, L::STAGE_LIGHT7_Y)), module, M::STAGE_LIGHT7));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT8_X, L::STAGE_LIGHT8_Y)), module, M::STAGE_LIGHT8));
        addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(L::STAGE_LIGHT9_X, L::STAGE_LIGHT9_Y)), module, M::STAGE_LIGHT9));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::OSC_ON_X, L::OSC_ON_Y)), module, M::OSC_ON));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::OSC_OFF_X, L::OSC_OFF_Y)), module, M::OSC_OFF));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::OSC_CV_X, L::OSC_CV_Y)), module, M::OSC_CV));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN1_X, L::SIN1_Y)), module, M::SIN1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN2_X, L::SIN2_Y)), module, M::SIN2));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN3_X, L::SIN3_Y)), module, M::SIN3));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN4_X, L::SIN4_Y)), module, M::SIN4));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN5_X, L::SIN5_Y)), module, M::SIN5));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN6_X, L::SIN6_Y)), module, M::SIN6));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN7_X, L::SIN7_Y)), module, M::SIN7));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN8_X, L::SIN8_Y)), module, M::SIN8));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SIN9_X, L::SIN9_Y)), module, M::SIN9));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::SHIFT_X, L::SHIFT_Y)), module, M::SHIFT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OSC_OUT_X, L::OSC_OUT_Y)), module, M::OSC_OUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT1_X, L::SOUT1_Y)), module, M::SOUT1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT2_X, L::SOUT2_Y)), module, M::SOUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT3_X, L::SOUT3_Y)), module, M::SOUT3));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT4_X, L::SOUT4_Y)), module, M::SOUT4));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT5_X, L::SOUT5_Y)), module, M::SOUT5));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT6_X, L::SOUT6_Y)), module, M::SOUT6));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT7_X, L::SOUT7_Y)), module, M::SOUT7));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT8_X, L::SOUT8_Y)), module, M::SOUT8));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::SOUT9_X, L::SOUT9_Y)), module, M::SOUT9));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::ROWA1_X, L::ROWA1_Y)), module, M::ROWA1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::ROWA2_X, L::ROWA2_Y)), module, M::ROWA2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::ROWB1_X, L::ROWB1_Y)), module, M::ROWB1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::ROWB2_X, L::ROWB2_Y)), module, M::ROWB2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::ROWC1_X, L::ROWC1_Y)), module, M::ROWC1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::ROWC2_X, L::ROWC2_Y)), module, M::ROWC2));
    }

    void appendContextMenu(Menu* menu) override {
        auto* m = getModule<JuniataModule>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createIndexSubmenuItem(
            "Stop stage behaviour", {"STOP only stops the oscillator", "STOP also blocks SHIFT"},
            [=]() { return m->stopMode == Seq960::StopMode::OscOnly ? 0 : 1; },
            [=](int i) { m->stopMode = i == 0 ? Seq960::StopMode::OscOnly : Seq960::StopMode::BlockShift; }));
        k280vcv::appendConfidence(menu, Seq960::kConfidence,
                                  "Row C timing and oscillator duty fitted to bench measurements.");
    }
};

Model* modelJuniata = createModel<JuniataModule, JuniataWidget>("Juniata");
