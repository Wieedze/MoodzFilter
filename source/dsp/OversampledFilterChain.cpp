#include "dsp/OversampledFilterChain.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace moodz::dsp
{
    static_assert (DryWetMix::maxSegment >= Oversampler::chunkSize, "the chain hands the mix segments of up to a chunk");
    static_assert (OversampledFilterChain::latencySamples <= WholeSampleDelay::maxDelay, "the dry path is delayed by the latency");

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
        lfo.prepare (rate, maxBlockSize);
        dryWet.prepare (rate, maxBlockSize, latencySamples);
        const double gridSeconds = modulationGridSamples / rate;
        // One-pole low-pass, sampled RC: y += (1 - exp (-T / tau)) (x - y), per step T of the grid
        slewCoefficient = static_cast<float> (1.0 - std::exp (-gridSeconds / lfoSlewSeconds));
        lfoDepthRampSteps = std::max (1, static_cast<int> (std::lround (lfoDepthRampSeconds / gridSeconds)));
        reset();
    }

    void OversampledFilterChain::reset() noexcept MOODZ_NONBLOCKING
    {
        dryWet.reset();
        lfoDepth = lfoDepthTarget;
        lfoDepthStep = 0.0f;
        lfoDepthStepsLeft = 0;
        gridPosition = 0;
        lfo.reset();
        slewedValue = lfo.value();
        slewStartsOnNextProcess = true;
        applyCutoff();
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

    void OversampledFilterChain::setMix (float amount) noexcept MOODZ_NONBLOCKING
    {
        dryWet.setAmount (amount);
    }

    void OversampledFilterChain::setLfoShapeIndex (int index) noexcept MOODZ_NONBLOCKING
    {
        lfo.setShapeIndex (index);
    }

    void OversampledFilterChain::setLfoRateIndex (int index) noexcept MOODZ_NONBLOCKING
    {
        lfo.setRateIndex (index);
    }

    void OversampledFilterChain::setLfoDepthOctaves (float octaves) noexcept MOODZ_NONBLOCKING
    {
        if (!std::isfinite (octaves))
            return;
        // Called every block: a ramp starts only when the target really changes
        const float clamped = std::clamp (octaves, -maxLfoDepthOctaves, maxLfoDepthOctaves);
        if (clamped == lfoDepthTarget)
            return;
        // Back from rest the slew starts on the LFO's value: it was not followed while nothing used it
        if (!lfoIsActive())
            slewedValue = lfo.value();
        lfoDepthTarget = clamped;
        lfoDepthStep = (lfoDepthTarget - lfoDepth) / static_cast<float> (lfoDepthRampSteps);
        lfoDepthStepsLeft = lfoDepthRampSteps;
    }

    void OversampledFilterChain::setTempoBpm (double bpm) noexcept MOODZ_NONBLOCKING
    {
        lfo.setTempoBpm (bpm);
    }

    void OversampledFilterChain::syncLfoToPosition (double quarterNotes) noexcept MOODZ_NONBLOCKING
    {
        lfo.syncToPosition (quarterNotes);
    }

    float OversampledFilterChain::modulatedCutoffHz() const noexcept
    {
        return modulatedCutoff;
    }

    bool OversampledFilterChain::lfoIsActive() const noexcept
    {
        return lfoDepth != 0.0f || lfoDepthStepsLeft > 0;
    }

    void OversampledFilterChain::updateModulation() noexcept MOODZ_NONBLOCKING
    {
        if (lfoDepthStepsLeft > 0)
        {
            --lfoDepthStepsLeft;
            lfoDepth = lfoDepthStepsLeft == 0 ? lfoDepthTarget : lfoDepth + lfoDepthStep;
        }
        slewedValue += slewCoefficient * (lfo.value() - slewedValue);
        applyCutoff();
    }

    void OversampledFilterChain::applyCutoff() noexcept MOODZ_NONBLOCKING
    {
        // At depth 0 nothing is computed, so the cutoff is the parameter itself. An octave is a factor of 2.
        modulatedCutoff = lfoDepth == 0.0f ? cutoffHz : cutoffHz * std::pow (2.0f, lfoDepth * slewedValue);
    }

    void OversampledFilterChain::startPath (std::size_t index) noexcept MOODZ_NONBLOCKING
    {
        auto& path = paths[index];
        path.drive.setDriveDb (driveDb);
        path.drive.setCurveIndex (driveCurveIndex);
        path.drive.reset();
        path.filters.setCutoffHz (modulatedCutoff);
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
            paths[index].filters.setCutoffHz (modulatedCutoff);
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

        for (std::size_t c = 0; c < numChannels; ++c)
            for (int i = 0; i < count; ++i)
            {
                auto& sample = channels[c][static_cast<std::size_t> (i)];
                // False for NaN too
                if (!(std::abs (sample) <= maxInputSample))
                    sample = 0.0f;
            }

        if (slewStartsOnNextProcess && count > 0)
        {
            slewStartsOnNextProcess = false;
            slewedValue = lfo.value();
            applyCutoff();
        }

        int start = 0;
        while (start < count)
        {
            // At rest the blocks are not cut, and a cutoff change applies at once, as without an LFO
            const bool modulating = lfoIsActive();
            if (!modulating)
                applyCutoff();
            else if (gridPosition == 0)
                updateModulation();
            // Also between chunks: a factor held during a crossfade starts the sample it ends,
            // so the output does not depend on the block size
            startPendingCrossfade();
            const bool crossfading = crossfadePosition < crossfadeLength;
            int length = std::min (Oversampler::chunkSize, count - start);
            // Cut on the modulation grid, so the cutoff moves at the same samples whatever the block
            if (modulating)
                length = std::min (length, modulationGridSamples - gridPosition);
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
            // The input after the guard, for the dry path
            dryWet.captureDry ({ incoming.data(), numChannels }, length);
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
            dryWet.mix ({ incoming.data(), numChannels }, length);
            lfo.advance (length);
            gridPosition = (gridPosition + length) % modulationGridSamples;
            start += length;
        }
    }
}
