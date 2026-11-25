// Shared VCV helpers for Keystone 280 modules.
#pragma once
#include <cmath>
#include <string>

#include "Confidence.hpp"
#include "Knob.hpp"
#include "plugin.hpp"

namespace k280vcv {

// "2 o'clock", "10:30" ... nearest half hour on the book's clock-face scale.
inline std::string clockString(float knob) {
    float h = std::round(k280::clockFromKnob(knob) * 2.f) / 2.f;
    int hour = int(h);
    bool half = (h - hour) > 0.25f;
    int h12 = hour > 12 ? hour - 12 : hour;
    return half ? string::f("%d:30", h12) : string::f("%d o'clock", h12);
}

// Knob whose tooltip also shows the clock position, e.g. "Frequency: +1.20 oct (2 o'clock)".
struct ClockQuantity : ParamQuantity {
    std::string getString() override {
        return ParamQuantity::getString() + "  (" + clockString(getScaledValue()) + ")";
    }
};

// Set the travel of a rotary switch drawn with fewer degrees than a normal knob.
template <class TKnob>
TKnob* createSnapKnob(Vec pos, Module* m, int id, float minDeg, float maxDeg) {
    TKnob* k = createParamCentered<TKnob>(pos, m, id);
    k->snap = true;
    k->minAngle = minDeg * float(M_PI) / 180.f;
    k->maxAngle = maxDeg * float(M_PI) / 180.f;
    return k;
}

inline void appendConfidence(Menu* menu, k280::Confidence c, const char* why) {
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuLabel(string::f("Model confidence: %s", k280::confidenceName(c))));
    menu->addChild(createMenuLabel(why));
}

}  // namespace k280vcv
