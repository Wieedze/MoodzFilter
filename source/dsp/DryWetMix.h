#pragma once

#include "dsp/Realtime.h"
#include "dsp/WholeSampleDelay.h"

#include <array>
#include <span>

namespace moodz::dsp
{
    // Blends the input with the processed signal (task 0017). The dry part is delayed by the chain's
    // latency, so the two are aligned. Works on segments of at most maxSegment samples.
    class DryWetMix
    {
    public:
        static constexpr int maxSegment = 256;
        static constexpr double rampSeconds = 0.02;

        // The dry path's delay is set apart: it is the chain's latency.
        void prepare (double sampleRate, int maxBlockSize, int delaySamples) noexcept;
        // Jumps to the target; clears the dry path.
        void reset() noexcept MOODZ_NONBLOCKING;

        // 0 is all dry, 1 all wet. Clamped; a non-finite value is ignored.
        void setAmount (float amount) noexcept MOODZ_NONBLOCKING;

        // Before processing: keeps the input of the segment, delayed.
        void captureDry (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;
        // After processing: replaces the channels by the blend. Same segment as captureDry.
        void mix (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;

    private:
        WholeSampleDelay delay;
        std::array<std::array<float, maxSegment>, 2> dry {};
        int rampLength = 960;
        float target = 1.0f;
        float current = 1.0f;
        float rampStart = 1.0f;
        float step = 0.0f;
        int samplesLeft = 0;
    };
}
