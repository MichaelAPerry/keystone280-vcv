// Band-limited steps (2-sample polyBLEP) with one sample of latency.
// For each output: report every discontinuity during the sample with addStep(height, frac),
// where frac is when it happened within the sample interval (0 = start, 1 = end), then call
// emit(naive value at the end of the interval). Returns the corrected previous sample.
#pragma once

namespace k280 {

class BlepChannel {
public:
    void addStep(float height, float frac) {
        float d = 1.f - frac;  // samples elapsed between the step and the current sample point
        prevCorr_ += height * 0.5f * d * d;
        curCorr_ += height * (d - 0.5f * d * d - 0.5f);
    }
    float emit(float naive) {
        float out = prev_ + prevCorr_;
        prev_ = naive + curCorr_;
        prevCorr_ = curCorr_ = 0.f;
        return out;
    }

private:
    float prev_ = 0.f, prevCorr_ = 0.f, curCorr_ = 0.f;
};

}  // namespace k280
