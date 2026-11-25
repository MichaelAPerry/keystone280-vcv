// VCV wrapper for the 911 envelope generator. DSP lives in engine/modules/Env911.hpp.
#include "common.hpp"
#include "layout/Lehigh.hpp"
#include "modules/Env911.hpp"

namespace L = layout_Lehigh;
using k280::Env911;

struct LehighModule : Module {
    // Append only: patch files store ids by number.
    enum ParamId { T1, T2, ESUS, T3, PARAMS_LEN };
    enum InputId { STRIG, INPUTS_LEN };
    enum OutputId { OUT, OUTPUTS_LEN };
    enum LightId { LIGHTS_LEN };

    Env911 env;

    template <float (Env911::*Taper)(float) const>
    struct TaperQuantity : k280vcv::ClockQuantity {
        // The taper is monotonic: invert it by bisection.
        void setDisplayValue(float d) override {
            if (!std::isfinite(d)) return;
            Env911 e;
            float lo = 0.f, hi = 1.f;
            for (int i = 0; i < 24; i++) {
                float mid = 0.5f * (lo + hi);
                ((e.*Taper)(mid) < d ? lo : hi) = mid;
            }
            setImmediateValue(0.5f * (lo + hi));
        }
        float getDisplayValue() override { return (Env911().*Taper)(getValue()); }
    };

    // Time knob shown as ms below one second, seconds above.
    template <float (Env911::*Seconds)(float) const>
    struct TimeQuantity : TaperQuantity<Seconds> {
        std::string getDisplayValueString() override {
            float s = this->getDisplayValue();
            return s < 1.f ? string::f("%.0f ms", s * 1000.f) : string::f("%.2f s", s);
        }
        std::string getUnit() override { return ""; }
    };

    LehighModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam<TimeQuantity<&Env911::attackSeconds>>(T1, 0.f, 1.f, 0.f, "T1 attack");
        configParam<TimeQuantity<&Env911::timeSeconds>>(T2, 0.f, 1.f, 0.35f, "T2 decay");
        configParam<TaperQuantity<&Env911::sustainVolts>>(
            ESUS, 0.f, 1.f, 0.5f,
            string::f("E SUS sustain level (%.1f V max)", env.sustainVolts(1.f)), " V");
        configParam<TimeQuantity<&Env911::timeSeconds>>(T3, 0.f, 1.f, 0.35f, "T3 release");

        configInput(STRIG, "S-trig (active low, hold low until release)");
        configOutput(OUT, "Envelope");
    }

    void process(const ProcessArgs& args) override {
        Env911::Params p;
        p.t1 = params[T1].getValue();
        p.t2 = params[T2].getValue();
        p.esus = params[ESUS].getValue();
        p.t3 = params[T3].getValue();

        Env911::Inputs in;
        in.strig = inputs[STRIG].getVoltage();
        in.strigConnected = inputs[STRIG].isConnected();

        outputs[OUT].setVoltage(env.process(args.sampleRate, p, in).out);
    }
};

struct LehighWidget : ModuleWidget {
    LehighWidget(LehighModule* module) {
        using M = LehighModule;
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Lehigh.svg")));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::T1_X, L::T1_Y)), module, M::T1));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::T2_X, L::T2_Y)), module, M::T2));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::T3_X, L::T3_Y)), module, M::T3));
        addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(L::ESUS_X, L::ESUS_Y)), module, M::ESUS));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(L::STRIG_X, L::STRIG_Y)), module, M::STRIG));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(L::OUT_X, L::OUT_Y)), module, M::OUT));
    }

    void appendContextMenu(Menu* menu) override {
        k280vcv::appendConfidence(menu, Env911::kConfidence, "Decay and release share the bench-fit taper. Attack has its own taper.");
    }
};

Model* modelLehigh = createModel<LehighModule, LehighWidget>("Lehigh");
