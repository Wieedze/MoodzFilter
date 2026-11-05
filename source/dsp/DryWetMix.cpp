#include "dsp/DryWetMix.h"

#include <algorithm>
#include <cmath>

namespace moodz::dsp
{
    void DryWetMix::prepare (double sampleRate, int /* maxBlockSize */, int delaySamples) noexcept
    {
        const double rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        rampLength = std::max (1, static_cast<int> (std::lround (rampSeconds * rate)));
        delay.prepare (delaySamples);
        reset();
    }

    void DryWetMix::reset() noexcept MOODZ_NONBLOCKING
    {
        current = rampStart = target;
        step = 0.0f;
        samplesLeft = 0;
        delay.reset();
    }

    void DryWetMix::setAmount (float amount) noexcept MOODZ_NONBLOCKING
    {
        if (!std::isfinite (amount))
            return;
        // Called every block: a ramp starts only when the target really changes
        const float clamped = std::clamp (amount, 0.0f, 1.0f);
        if (clamped == target)
            return;
        target = clamped;
        rampStart = current;
        step = (target - current) / static_cast<float> (rampLength);
        samplesLeft = rampLength;
    }

    void DryWetMix::captureDry (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        const auto numChannels = std::min (channels.size(), dry.size());
        const auto size = static_cast<std::size_t> (std::clamp (numSamples, 0, maxSegment));
        std::array<std::span<float>, 2> kept {};
        for (std::size_t c = 0; c < numChannels; ++c)
        {
            kept[c] = std::span<float> { dry[c] }.first (size);
            std::copy_n (channels[c].begin(), size, kept[c].begin());
        }
        delay.process ({ kept.data(), numChannels }, static_cast<int> (size));
    }

    void DryWetMix::mix (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        // All wet and at rest: the output is the wet part untouched
        if (samplesLeft == 0 && current >= 1.0f)
            return;
        const auto numChannels = std::min (channels.size(), dry.size());
        const int count = std::clamp (numSamples, 0, maxSegment);
        for (int i = 0; i < count; ++i)
        {
            if (samplesLeft > 0)
            {
                --samplesLeft;
                // From the start of the ramp, so float rounding does not drift; the last sample lands on the target
                current = samplesLeft == 0 ? target : rampStart + step * static_cast<float> (rampLength - samplesLeft);
            }
            const auto index = static_cast<std::size_t> (i);
            // Linear: dry and wet are aligned and correlated, so an equal-power law would bump the middle.
            // All dry takes nothing of the wet sample, even a non-finite one: 0 x NaN is NaN.
            for (std::size_t c = 0; c < numChannels; ++c)
                channels[c][index] = current <= 0.0f ? dry[c][index] : current * channels[c][index] + (1.0f - current) * dry[c][index];
        }
    }
}
