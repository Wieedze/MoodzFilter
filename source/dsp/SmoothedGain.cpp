#include "dsp/SmoothedGain.h"

#include <algorithm>
#include <cmath>

namespace moodz::dsp
{
    namespace
    {
        // Decibels to linear amplitude: g = 10^(dB / 20) (definition of the decibel for amplitudes)
        float decibelsToGain (float gainDb) noexcept
        {
            return std::pow (10.0f, gainDb / 20.0f);
        }
    }

    void SmoothedGain::prepare (double sampleRate, int /* maxBlockSize */) noexcept
    {
        // A non-finite, non-positive or absurd rate would make lround and the cast to int
        // undefined: fall back to a one-sample ramp, which is still click-free at any real rate.
        const bool validRate = sampleRate > 0.0 && sampleRate < 10'000'000.0;
        rampLengthSamples = validRate ? std::max (1, static_cast<int> (std::lround (sampleRate * rampSeconds))) : 1;
        reset();
    }

    void SmoothedGain::reset() noexcept
    {
        currentGain = targetGain;
        step = 0.0f;
        samplesLeft = 0;
    }

    void SmoothedGain::setTargetDb (float gainDb) noexcept
    {
        if (!std::isfinite (gainDb))
            return;

        // Called every block: recompute the linear gain only when the target really changes
        const float clampedDb = std::clamp (gainDb, minDb, maxDb);
        if (clampedDb == targetDb)
            return;

        // A new target restarts a full-length ramp from wherever the gain is now
        targetDb = clampedDb;
        targetGain = decibelsToGain (clampedDb);
        samplesLeft = rampLengthSamples;
        step = (targetGain - currentGain) / static_cast<float> (rampLengthSamples);
    }

    void SmoothedGain::process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (samplesLeft > 0)
            {
                --samplesLeft;
                // Land exactly on the target at the end of the ramp, so no rounding drift remains
                currentGain = samplesLeft == 0 ? targetGain : currentGain + step;
            }

            const auto index = static_cast<size_t> (i);
            for (const auto& channel : channels)
                channel[index] *= currentGain;
        }
    }
}
