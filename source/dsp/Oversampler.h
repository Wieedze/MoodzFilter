#pragma once

#include "dsp/Realtime.h"

#include <signalsmith-dsp/rates.h>

#include <array>
#include <span>

namespace moodz::dsp
{
    // Up and down sampling by 1, 2 or 4 around a non-linear stage (ADR 0009): linear phase, a
    // whole number of samples of round-trip latency, no JUCE. 4x is two 2x stages.
    // Signalsmith DSP rates.h (MIT, v1.7.3), Oversampler2xFIR.
    class Oversampler
    {
    public:
        // Most samples at the base rate in one up or down call
        static constexpr int chunkSize = 256;
        // Round trip at 4x, the largest: the plugin's constant latency (ADR 0009)
        static constexpr int maxLatencySamples = 40;
        static constexpr std::array<const char*, 3> factorLabels { "Off", "2x", "4x" };
        static constexpr int factorCount = static_cast<int> (factorLabels.size());
        static constexpr int defaultFactorIndex = 1; // SPEC: 2x

        // Off the audio thread: index 0 Off, 1 2x, 2 4x, bounded. Allocates.
        void prepare (int factorIndex);
        void reset() noexcept MOODZ_NONBLOCKING;

        int factor() const noexcept;
        int latencySamples() const noexcept;

        // input holds at most chunkSize samples; returns the signal at the high rate, factor()
        // times longer. Channel 0 or 1.
        std::span<float> up (int channel, std::span<const float> input) noexcept MOODZ_NONBLOCKING;
        // After up with the same length: the high rate signal back into output.
        void down (int channel, std::span<float> output) noexcept MOODZ_NONBLOCKING;

    private:
        signalsmith::rates::Oversampler2xFIR<float> first, second;
        std::array<std::array<float, chunkSize>, 2> plain {};
        int stages = 0;
    };
}
