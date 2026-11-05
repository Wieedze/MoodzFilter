#include "dsp/OversampledFilterChain.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace moodz::dsp
{
    void OversampledFilterChain::prepare (double sampleRate, int maxBlockSize)
    {
        const double rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        for (std::size_t i = 0; i < paths.size(); ++i)
        {
            auto& path = paths[i];
            path.oversampler.prepare (static_cast<int> (i));
            path.drive.prepare (rate * path.oversampler.factor(), maxBlockSize * path.oversampler.factor());
            path.filters.prepare (rate * path.oversampler.factor(), maxBlockSize * path.oversampler.factor());
            path.delay.prepare (latencySamples - path.oversampler.latencySamples());
        }
        crossfadeLength = std::max (1, static_cast<int> (std::lround (crossfadeSeconds * rate)));
        reset();
    }

    void OversampledFilterChain::reset() noexcept MOODZ_NONBLOCKING
    {
        current = outgoing = pending;
        startPath (current);
        crossfadePosition = crossfadeLength;
    }

    void OversampledFilterChain::setFactorIndex (int index) noexcept MOODZ_NONBLOCKING
    {
        pending = static_cast<std::size_t> (std::clamp (index, 0, Oversampler::factorCount - 1));
    }

    void OversampledFilterChain::select (FilterChoice newChoice) noexcept MOODZ_NONBLOCKING
    {
        choice = newChoice;
    }

    void OversampledFilterChain::setCutoffHz (float hz) noexcept MOODZ_NONBLOCKING
    {
        if (std::isfinite (hz))
            cutoffHz = hz;
    }

    void OversampledFilterChain::setResonance (float amount) noexcept MOODZ_NONBLOCKING
    {
        if (std::isfinite (amount))
            resonance = amount;
    }

    void OversampledFilterChain::setDriveDb (float db) noexcept MOODZ_NONBLOCKING
    {
        if (std::isfinite (db))
            driveDb = db;
    }

    void OversampledFilterChain::setDriveCurveIndex (int index) noexcept MOODZ_NONBLOCKING
    {
        driveCurveIndex = index;
    }

    void OversampledFilterChain::startPath (std::size_t index) noexcept MOODZ_NONBLOCKING
    {
        auto& path = paths[index];
        path.drive.setDriveDb (driveDb);
        path.drive.setCurveIndex (driveCurveIndex);
        path.drive.reset();
        path.filters.setCutoffHz (cutoffHz);
        path.filters.setResonance (resonance);
        path.filters.select (choice);
        path.filters.reset();
        path.oversampler.reset();
        path.delay.reset();
    }

    void OversampledFilterChain::startPendingCrossfade() noexcept MOODZ_NONBLOCKING
    {
        // A factor chosen during a crossfade waits for it to end
        if (pending != current && crossfadePosition >= crossfadeLength)
        {
            outgoing = current;
            current = pending;
            startPath (current);
            paths[current].drive.takeDcEstimateFrom (paths[outgoing].drive);
            crossfadePosition = 0;
        }
        paths[current].filters.select (choice);
        for (auto index : { current, outgoing })
        {
            paths[index].filters.setCutoffHz (cutoffHz);
            paths[index].filters.setResonance (resonance);
            paths[index].drive.setDriveDb (driveDb);
            paths[index].drive.setCurveIndex (driveCurveIndex);
        }
    }

    void OversampledFilterChain::render (Path& path, std::span<std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        std::array<std::span<float>, 2> high {};
        for (std::size_t c = 0; c < channels.size(); ++c)
            high[c] = path.oversampler.up (static_cast<int> (c), channels[c].first (static_cast<std::size_t> (numSamples)));
        path.drive.process ({ high.data(), channels.size() }, numSamples * path.oversampler.factor());
        path.filters.process ({ high.data(), channels.size() }, numSamples * path.oversampler.factor());
        for (std::size_t c = 0; c < channels.size(); ++c)
            path.oversampler.down (static_cast<int> (c), channels[c].first (static_cast<std::size_t> (numSamples)));
        path.delay.process (channels, numSamples);
    }

    void OversampledFilterChain::process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        if (channels.empty())
            return;
        const auto numChannels = std::min<std::size_t> (channels.size(), scratch.size());
        auto available = channels[0].size();
        for (std::size_t c = 1; c < numChannels; ++c)
            available = std::min (available, channels[c].size());
        const int count = std::min (numSamples, static_cast<int> (std::min (available, static_cast<std::size_t> (std::numeric_limits<int>::max()))));

        int start = 0;
        while (start < count)
        {
            // Also between chunks: a factor held during a crossfade starts the sample it ends,
            // so the output does not depend on the block size
            startPendingCrossfade();
            const bool crossfading = crossfadePosition < crossfadeLength;
            int length = std::min (Oversampler::chunkSize, count - start);
            if (crossfading)
                length = std::min (length, crossfadeLength - crossfadePosition);
            const auto size = static_cast<std::size_t> (length);

            std::array<std::span<float>, 2> incoming {}, fading {};
            for (std::size_t c = 0; c < numChannels; ++c)
            {
                incoming[c] = channels[c].subspan (static_cast<std::size_t> (start), size);
                fading[c] = std::span<float> { scratch[c] }.first (size);
                if (crossfading)
                    std::copy (incoming[c].begin(), incoming[c].end(), fading[c].begin());
            }
            if (crossfading)
                render (paths[outgoing], { fading.data(), numChannels }, length);
            render (paths[current], { incoming.data(), numChannels }, length);

            if (crossfading)
            {
                // Equal gain, as in CrossfadingFilterBank: the two aligned paths filter the same
                // input. 0 during the pre-roll, then the ramp over the rest of the crossfade.
                const int preRoll = std::min (preRollSamples, crossfadeLength - 1);
                const float step = 1.0f / static_cast<float> (crossfadeLength - preRoll);
                for (std::size_t i = 0; i < size; ++i)
                {
                    const float gain = static_cast<float> (std::max (crossfadePosition + static_cast<int> (i) - preRoll, 0)) * step;
                    for (std::size_t c = 0; c < numChannels; ++c)
                        incoming[c][i] = incoming[c][i] * gain + fading[c][i] * (1.0f - gain);
                }
                crossfadePosition += length;
            }
            start += length;
        }
    }
}
