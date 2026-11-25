// How far each module's behaviour can be trusted. A patch's confidence is its weakest module's.
#pragma once

namespace k280 {

enum class Confidence {
    Schematic,  // modelled from the circuit schematic
    Traced,     // modelled from a community-traced circuit
    Ported,     // ported from open-source firmware
    Manual,     // behaviour and numbers from the manufacturer's manual only
    StandIn,    // functional stand-in, not a model of the real module
};

inline const char* confidenceName(Confidence c) {
    switch (c) {
        case Confidence::Schematic: return "schematic";
        case Confidence::Traced: return "traced";
        case Confidence::Ported: return "ported";
        case Confidence::Manual: return "manual";
        case Confidence::StandIn: return "stand-in";
    }
    return "?";
}

}  // namespace k280
