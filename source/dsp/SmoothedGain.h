#pragma once

#include "dsp/Realtime.h"

#include <span>

namespace moodz::dsp
{
    // Gain in decibels applied to linked channels, with a linear ramp of the linear gain
    // over rampSeconds whenever the target changes, so automation does not click.
    class SmoothedGain
    {
    public:
        // SPEC v1 amended by ADR 0004: from silence to +6 dB. At silenceFloorDb and below,
        // the gain is exactly 0 (displayed -inf by the plugin layer).
        static constexpr float silenceFloorDb = -60.0f;
        static constexpr float maxDb = 6.0f;
        static constexpr double rampSeconds = 0.02;

        void prepare (double sampleRate, int maxBlockSize) noexcept;

        // Jumps the current gain to the target: no ramp after reset().
        void reset() noexcept;

        // Clamped to [silenceFloorDb, maxDb]; a non-finite value is ignored.
        void setTargetDb (float gainDb) noexcept MOODZ_NONBLOCKING;

        // Every channel span must hold at least numSamples samples. The same gain
        // is applied to all channels (linked stereo, SPEC).
        void process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;

    private:
        int rampLengthSamples = 1;
        float currentGain = 1.0f;
        float rampStartGain = 1.0f;
        float targetDb = 0.0f;
        float targetGain = 1.0f;
        float step = 0.0f;
        int samplesLeft = 0;
    };
}
