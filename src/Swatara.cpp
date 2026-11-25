// VCV wrapper for the Toppobrillo Quantimator. DSP lives in engine/modules/Quantimator.hpp.
// Port and param ids are the layout ids (the book's jack keys, manual/tools/rack.py). Append only:
// patch files store ids by number.
#include <atomic>

#include "common.hpp"
#include "layout/Swatara.hpp"
#include "modules/Quantimator.hpp"

namespace L = layout_Swatara;
using k280::Quantimator;

struct SwataraModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { MODE, TRANSPOSE, CHORD_SCALE, CS_ATT, PARAMS_LEN };
    enum InputId { IN, TRIG, HOLD, TRANS, CSCV, INPUTS_LEN };
    enum OutputId { OUT1, OUT2, OUT3, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Quantimator quant;
    // Positive display numbers are chords, negative numbers are scales, and zero is HOLD.
    std::atomic<int> displayType{1};
    bool holdHigh = false;

    Quantimator::Params currentParams() {
        Quantimator::Params p;
        p.scaleMode = params[MODE].getValue() > 0.5f;
        p.chordScale = params[CHORD_SCALE].getValue();
        p.csAtt = params[CS_ATT].getValue();
        p.transpose = params[TRANSPOSE].getValue();
        return p;
    }

    // Names the selected type in the current MODE, e.g. "17: bali pelog".
    struct TypeQuantity : ParamQuantity {
        std::string getDisplayValueString() override {
            auto* m = dynamic_cast<SwataraModule*>(module);
            if (!m) return ParamQuantity::getDisplayValueString();
            const Quantimator::Params p = m->currentParams();
            const float cv = m->inputs[CSCV].isConnected() ? k280vcv::inputVoltage(m->inputs[CSCV]) : 0.f;
            const int i = m->quant.typeIndex(p, cv);
            return string::f("%d: %s", i, p.scaleMode ? Quantimator::scaleName(i) : Quantimator::chordName(i));
        }
        std::string getUnit() override { return ""; }
    };

    // Whole semitones, as the engine rounds the knob.
    struct TransposeQuantity : ParamQuantity {
        static constexpr float span = k280::QuantimatorCalibration::defaultTransposeKnobSpan;
        float getDisplayValue() override { return std::round(getValue() * span); }
        void setDisplayValue(float d) override {
            if (std::isfinite(d)) setImmediateValue(math::clamp(std::round(d) / span, 0.f, 1.f));
        }
        std::string getUnit() override { return " semitones"; }
    };

    SwataraModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configSwitch(MODE, 0.f, 1.f, 0.f, "Mode", {"Chord", "Scale"});
        configParam<TransposeQuantity>(TRANSPOSE, 0.f, 1.f, 0.5f, "Transpose");
        configParam<TypeQuantity>(CHORD_SCALE, 0.f, 1.f, 0.f, "Chord / scale type");
        configParam(CS_ATT, 0.f, 1.f, 1.f, "Chord / scale CV attenuator", " %", 0.f, 100.f);

        configInput(IN, "Pitch in (1 V/oct, 0 to +5 V)");
        configInput(TRIG, "Trigger (+1.5 V threshold; reads inputs only on a trigger)");
        configInput(HOLD, "Hold (+1.5 V threshold)");
        configInput(TRANS, "Transpose CV (1 V/oct, 0 to +5 V)");
        configInput(CSCV, "Chord / scale CV (5 V spans all 32 types)");
        configOutput(OUT1, "Out 1 (1 V/oct, 0 to +5 V)");
        configOutput(OUT2, "Out 2 (1 V/oct, 0 to +5 V)");
        configOutput(OUT3, "Out 3 (1 V/oct, 0 to +5 V)");
    }

    void process(const ProcessArgs& args) override {
        const Quantimator::Params p = currentParams();

        Quantimator::Inputs in;
        in.inV = k280vcv::inputVoltage(inputs[IN]);       in.inConnected = inputs[IN].isConnected();
        in.trigV = k280vcv::inputVoltage(inputs[TRIG]);   in.trigConnected = inputs[TRIG].isConnected();
        in.holdV = k280vcv::inputVoltage(inputs[HOLD]);   in.holdConnected = inputs[HOLD].isConnected();
        in.transV = k280vcv::inputVoltage(inputs[TRANS]); in.transConnected = inputs[TRANS].isConnected();
        in.cscvV = k280vcv::inputVoltage(inputs[CSCV]);   in.cscvConnected = inputs[CSCV].isConnected();

        Quantimator::Outputs o = quant.process(args.sampleRate, p, in);
        for (int i = 0; i < 3; i++) outputs[OUT1 + i].setVoltage(o.out[i]);
        const float holdThreshold = quant.cal.holdOnV - (holdHigh ? quant.cal.gateHysteresisV : 0.f);
        holdHigh = in.holdConnected && in.holdV >= holdThreshold;
        const int typeNumber = 1 + quant.typeIndex(p, in.cscvConnected ? in.cscvV : 0.f);
        const int selection = p.scaleMode ? -typeNumber : typeNumber;
        displayType.store(holdHigh && !in.trigConnected ? 0 : selection, std::memory_order_relaxed);
    }
};

namespace {
// The Quantimator manual shows a leading point for SCALE and a trailing BNK point
// for memory locations 17–32. HOLD without TRIG displays H.
struct TypeDisplay : TransparentWidget {
    SwataraModule* module = nullptr;

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
        if (!font) return;
        const int n = module ? module->displayType.load(std::memory_order_relaxed) : 18;
        nvgFontFaceId(args.vg, font->handle);
        const int index = std::abs(n) - 1;
        const auto label = n == 0 ? std::string("H") :
            string::f("%s%c%s", n < 0 ? "." : "", "0123456789ABCDEF"[index % 16], index >= 16 ? "." : "");
        const float fontSize = box.size.y * 0.8f;
        nvgFontSize(args.vg, fontSize);
        const float width = nvgTextBounds(args.vg, 0.f, 0.f, label.c_str(), nullptr, nullptr);
        if (width > box.size.x) nvgFontSize(args.vg, fontSize * box.size.x / width);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGB(0xff, 0x2a, 0x1a));
        nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, label.c_str(), nullptr);
    }
};
}  // namespace

struct SwataraWidget : ModuleWidget {
    SwataraWidget(SwataraModule* module) {
        using M = SwataraModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Swatara.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<CKSS>(mm2px(Vec(L::MODE_X, L::MODE_Y)), module, M::MODE));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::TRANSPOSE_X, L::TRANSPOSE_Y)), module, M::TRANSPOSE));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::CHORD_SCALE_X, L::CHORD_SCALE_Y)), module, M::CHORD_SCALE));
        auto* att = createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::CS_ATT_X, L::CS_ATT_Y)), module, M::CS_ATT);
        constexpr float radiansPerDegree = 0.01745329252f;
        att->minAngle = L::CS_ATT_MIN_DEG * radiansPerDegree;
        att->maxAngle = L::CS_ATT_MAX_DEG * radiansPerDegree;
        addParam(att);

        auto* display = createWidget<TypeDisplay>(mm2px(Vec(L::DISPLAY_X, L::DISPLAY_Y)));
        display->box.size = mm2px(Vec(L::DISPLAY_W, L::DISPLAY_H));
        display->module = module;
        addChild(display);

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::IN_X, L::IN_Y)), module, M::IN));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::TRIG_X, L::TRIG_Y)), module, M::TRIG));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::HOLD_X, L::HOLD_Y)), module, M::HOLD));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::TRANS_X, L::TRANS_Y)), module, M::TRANS));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::CSCV_X, L::CSCV_Y)), module, M::CSCV));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT1_X, L::OUT1_Y)), module, M::OUT1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT2_X, L::OUT2_Y)), module, M::OUT2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT3_X, L::OUT3_Y)), module, M::OUT3));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Quantimator::kConfidence,
                                  "Bench measurements cover chromatic tracking and transpose. Some interval tables are estimated.");
    }
};

Model* modelSwatara = createModel<SwataraModule, SwataraWidget>("Swatara");
