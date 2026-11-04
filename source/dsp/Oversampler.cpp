#include "dsp/Oversampler.h"

#include <algorithm>

namespace moodz::dsp
{
    namespace
    {
        // Oversampler2xFIR takes half its round-trip latency, in samples of its lower rate. First
        // stage: pass band to 0.43 x the base rate (library default), 32 samples. Second stage:
        // its input stops at about half of its own band, so a shorter kernel with a wider
        // transition is enough: 16 samples at 2x, a whole 8 at the base rate. Total 40.
        constexpr int firstHalfLatency = 16, secondHalfLatency = 8;
        constexpr double firstPassBand = 0.43, secondPassBand = 0.25;
    }

    void Oversampler::prepare (int factorIndex)
    {
        stages = std::clamp (factorIndex, 0, factorCount - 1);
        if (stages >= 1)
            first.resize (static_cast<int> (plain.size()), chunkSize, firstHalfLatency, firstPassBand);
        if (stages == 2)
            second.resize (static_cast<int> (plain.size()), chunkSize * 2, secondHalfLatency, secondPassBand);
        reset();
    }

    void Oversampler::reset() noexcept MOODZ_NONBLOCKING
    {
        // Same size: assign writes in place, it does not allocate
        if (stages >= 1)
            first.reset();
        if (stages == 2)
            second.reset();
    }

    int Oversampler::factor() const noexcept
    {
        return 1 << stages;
    }

    int Oversampler::latencySamples() const noexcept
    {
        return (stages >= 1 ? first.latency() : 0) + (stages == 2 ? second.latency() / 2 : 0);
    }

    std::span<float> Oversampler::up (int channel, std::span<const float> input) noexcept MOODZ_NONBLOCKING
    {
        const int length = std::min (static_cast<int> (input.size()), chunkSize);
        const auto size = static_cast<std::size_t> (length);
        if (stages == 0)
        {
            auto& line = plain[static_cast<std::size_t> (channel)];
            std::copy_n (input.begin(), size, line.begin());
            return std::span<float> { line }.first (size);
        }
        first.upChannel (channel, input.data(), length);
        if (stages == 1)
            return { first[channel], size * 2 };
        second.upChannel (channel, first[channel], length * 2);
        return { second[channel], size * 4 };
    }

    void Oversampler::down (int channel, std::span<float> output) noexcept MOODZ_NONBLOCKING
    {
        const int length = std::min (static_cast<int> (output.size()), chunkSize);
        const auto size = static_cast<std::size_t> (length);
        if (stages == 0)
        {
            std::copy_n (plain[static_cast<std::size_t> (channel)].begin(), size, output.begin());
            return;
        }
        if (stages == 2)
            second.downChannel (channel, first[channel], length * 2);
        first.downChannel (channel, output.data(), length);
    }
}
