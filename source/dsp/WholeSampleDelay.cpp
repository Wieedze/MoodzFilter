#include "dsp/WholeSampleDelay.h"

#include <algorithm>

namespace moodz::dsp
{
    void WholeSampleDelay::prepare (int delaySamples) noexcept
    {
        delay = std::clamp (delaySamples, 0, maxDelay);
        reset();
    }

    void WholeSampleDelay::reset() noexcept MOODZ_NONBLOCKING
    {
        for (auto& line : lines)
            line.fill (0.0f);
        position = 0;
    }

    void WholeSampleDelay::process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        if (delay == 0 || channels.empty())
            return;
        const auto numChannels = std::min (channels.size(), lines.size());
        auto available = channels[0].size();
        for (std::size_t c = 1; c < numChannels; ++c)
            available = std::min (available, channels[c].size());
        const auto count = std::min (static_cast<std::size_t> (std::max (numSamples, 0)), available);

        for (std::size_t i = 0; i < count; ++i)
        {
            const auto at = static_cast<std::size_t> (position);
            for (std::size_t c = 0; c < numChannels; ++c)
                std::swap (channels[c][i], lines[c][at]);
            position = position + 1 == delay ? 0 : position + 1;
        }
    }
}
