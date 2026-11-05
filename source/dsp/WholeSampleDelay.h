#pragma once

#include "dsp/Realtime.h"

#include <array>
#include <span>

namespace moodz::dsp
{
    // Delays one or two channels by a whole number of samples, up to maxDelay (ADR 0009: it
    // completes Off and 2x to the constant latency, and delays the bypass path).
    class WholeSampleDelay
    {
    public:
        static constexpr int maxDelay = 64;

        // Bounded to [0, maxDelay]; clears the delay.
        void prepare (int delaySamples) noexcept;
        void reset() noexcept MOODZ_NONBLOCKING;
        void process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;

    private:
        std::array<std::array<float, maxDelay>, 2> lines {};
        int delay = 0;
        int position = 0;
    };
}
