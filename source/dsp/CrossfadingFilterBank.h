#pragma once

#include "dsp/FilterModels.h"
#include "dsp/MultimodeFilter.h"
#include "dsp/Realtime.h"

#include <array>
#include <span>

namespace moodz::dsp
{
    // Every filter variant, prepared once, and a linear crossfade when the choice moves from one
    // variant to another (task 0011, SPEC success criterion 3). The incoming variant starts from a
    // clean state; the outgoing one fades out over crossfadeSeconds. A choice made during a
    // crossfade waits for it to end, so at most two variants run at once.
    //
    // Linear (equal-gain) rather than equal-power: both variants filter the same input, so their
    // outputs are correlated and an equal-power fade would raise the level by up to 3 dB midway.
    class CrossfadingFilterBank
    {
    public:
        // Same ramp time as Gain out (SmoothedGain::rampSeconds)
        static constexpr double crossfadeSeconds = 0.02;

        // Off the audio thread: configures every variant (sst-filters allocates on first use).
        CrossfadingFilterBank();

        void prepare (double sampleRate, int maxBlockSize);
        // Jumps to the selected choice: no crossfade, every variant cleared.
        void reset() noexcept MOODZ_NONBLOCKING;

        // Takes effect at the start of the next process call; the index is bounded.
        void select (FilterChoice choice) noexcept MOODZ_NONBLOCKING;
        void setCutoffHz (float hz) noexcept MOODZ_NONBLOCKING;
        void setResonance (float amount) noexcept MOODZ_NONBLOCKING;

        // One or two channels; processes at most as many samples as the shortest span holds.
        void process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;

    private:
        // The outgoing variant renders into this scratch, in chunks of its size, whatever the
        // block size: no allocation, and a block of any length is processed whole
        static constexpr int scratchSize = 256;

        void startPendingCrossfade() noexcept MOODZ_NONBLOCKING;

        std::array<MultimodeFilter, filterVariantCount> filters;
        std::array<std::array<float, scratchSize>, 2> scratch {};
        FilterChoice pending {};
        std::size_t current = 0;
        std::size_t outgoing = 0;
        int crossfadeLength = 960;
        int crossfadePosition = 0; // samples done; crossfadeLength when no crossfade runs
        float cutoffHz = 1000.0f;
        float resonance = 0.0f;
    };
}
