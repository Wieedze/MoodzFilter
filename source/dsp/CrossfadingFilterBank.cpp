#include "dsp/CrossfadingFilterBank.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace moodz::dsp
{
    CrossfadingFilterBank::CrossfadingFilterBank()
    {
        const auto variants = filterVariants();
        for (std::size_t i = 0; i < filters.size(); ++i)
        {
            [[maybe_unused]] const bool valid = filters[i].configure (variants[i]);
            assert (valid);
        }
    }

    void CrossfadingFilterBank::prepare (double sampleRate, int maxBlockSize)
    {
        for (auto& filter : filters)
        {
            filter.prepare (sampleRate, maxBlockSize);
            filter.setCutoffHz (cutoffHz);
            filter.setResonance (resonance);
        }
        const double rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        crossfadeLength = std::max (1, static_cast<int> (std::lround (crossfadeSeconds * rate)));
        reset();
    }

    void CrossfadingFilterBank::reset() noexcept MOODZ_NONBLOCKING
    {
        for (auto& filter : filters)
            filter.reset();
        current = outgoing = pending.variant;
        filters[current].setExtra (pending.extra);
        crossfadePosition = crossfadeLength;
    }

    void CrossfadingFilterBank::select (FilterChoice choice) noexcept MOODZ_NONBLOCKING
    {
        pending = { std::min (choice.variant, filters.size() - 1), choice.extra };
    }

    void CrossfadingFilterBank::setCutoffHz (float hz) noexcept MOODZ_NONBLOCKING
    {
        if (std::isfinite (hz))
            cutoffHz = hz;
    }

    void CrossfadingFilterBank::setResonance (float amount) noexcept MOODZ_NONBLOCKING
    {
        if (std::isfinite (amount))
            resonance = amount;
    }

    void CrossfadingFilterBank::startPendingCrossfade() noexcept MOODZ_NONBLOCKING
    {
        // A choice made during a crossfade waits for it to end
        if (crossfadePosition < crossfadeLength)
            return;
        if (pending.variant != current)
        {
            outgoing = current;
            current = pending.variant;
            filters[current].reset();
            crossfadePosition = 0;
        }
        // The same variant with a new extra value: no crossfade, sst-filters smooths it (task 0010)
        filters[current].setExtra (pending.extra);
    }

    void CrossfadingFilterBank::process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        if (channels.empty())
            return;
        const auto numChannels = std::min<std::size_t> (channels.size(), scratch.size());
        auto available = channels[0].size();
        for (std::size_t c = 1; c < numChannels; ++c)
            available = std::min (available, channels[c].size());
        const int count = std::min (numSamples, static_cast<int> (std::min (available, static_cast<std::size_t> (std::numeric_limits<int>::max()))));
        if (count <= 0)
            return;

        startPendingCrossfade();
        for (auto index : { current, outgoing })
        {
            filters[index].setCutoffHz (cutoffHz);
            filters[index].setResonance (resonance);
        }

        int start = 0;
        while (start < count)
        {
            const auto offset = static_cast<std::size_t> (start);
            if (crossfadePosition >= crossfadeLength)
            {
                // No crossfade: the current variant alone, on the rest of the block
                std::array<std::span<float>, 2> rest {};
                for (std::size_t c = 0; c < numChannels; ++c)
                    rest[c] = channels[c].subspan (offset, static_cast<std::size_t> (count - start));
                filters[current].process ({ rest.data(), numChannels }, count - start);
                return;
            }

            // Crossfade: the outgoing variant renders a copy of the input into the scratch, the
            // incoming one renders in place, then out = in * g + scratch * (1 - g), g from 0 to
            // (length - 1) / length over the crossfade, so the outgoing variant is gone after it
            const int length = std::min ({ scratchSize, count - start, crossfadeLength - crossfadePosition });
            const auto size = static_cast<std::size_t> (length);
            std::array<std::span<float>, 2> incoming {}, fading {};
            for (std::size_t c = 0; c < numChannels; ++c)
            {
                incoming[c] = channels[c].subspan (offset, size);
                fading[c] = std::span<float> { scratch[c] }.first (size);
                std::copy (incoming[c].begin(), incoming[c].end(), fading[c].begin());
            }
            filters[outgoing].process ({ fading.data(), numChannels }, length);
            filters[current].process ({ incoming.data(), numChannels }, length);

            const float step = 1.0f / static_cast<float> (crossfadeLength);
            for (std::size_t i = 0; i < size; ++i)
            {
                const float gain = static_cast<float> (crossfadePosition + static_cast<int> (i)) * step;
                for (std::size_t c = 0; c < numChannels; ++c)
                    incoming[c][i] = incoming[c][i] * gain + fading[c][i] * (1.0f - gain);
            }
            crossfadePosition += length;
            start += length;
        }
    }
}
