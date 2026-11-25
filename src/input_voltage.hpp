#pragma once

#include <cmath>

namespace k280vcv {

// Keep malformed cable voltages out of stateful DSP and edge interpolation.
template<class Input>
float inputVoltage(Input& input) {
    const float volts = input.getVoltage();
    return std::isfinite(volts) ? volts : 0.f;
}

}  // namespace k280vcv
