#pragma once

#include "dsp/CrossfadingFilterBank.h"
#include "dsp/FilterModels.h"
#include "dsp/InputDrive.h"
#include "dsp/Oversampler.h"
#include "dsp/Realtime.h"
#include "dsp/SyncedLfo.h"
#include "dsp/WholeSampleDelay.h"

#include <array>
#include <span>

namespace moodz::dsp
{
    // The drive and the filter at 1x, 2x or 4x (tasks 0012, 0013, ADR 0009). Each factor owns an
    // input drive and a crossfading filter bank prepared at its rate, its oversampler and a delay
    // that completes it to the constant latency. A factor change resets the incoming path, lets it settle unheard for preRollSamples
    // (it starts empty, and its start transient would click), then crossfades the two aligned
    // outputs linearly, all within crossfadeSeconds; a change during a crossfade waits for it to end.
    class OversampledFilterChain
    {
    public:
        static constexpr int latencySamples = Oversampler::maxLatencySamples;
        static constexpr double crossfadeSeconds = CrossfadingFilterBank::crossfadeSeconds;
        // Twice the latency: the signal reaches the output, then the FIR's own ringing is over
        static constexpr int preRollSamples = 2 * latencySamples;
        // python3 -c "from math import log10; print(20*log10(256))": +48 dBFS, far above any real signal,
        // and the filters keep a finite state up to it (task 0014)
        static constexpr float maxInputSample = 256.0f;
        static constexpr float maxLfoDepthOctaves = 4.0f;
        // The cutoff follows the LFO every this many base samples, counted across blocks (task 0015)
        static constexpr int modulationGridSamples = 32;
        // Turns a square edge, a sawtooth reset or a loop into a short curve (task 0015)
        static constexpr double lfoSlewSeconds = 0.002;
        static constexpr double lfoDepthRampSeconds = 0.02;

        void prepare (double sampleRate, int maxBlockSize);
        // Jumps to the selected factor and choice: no crossfade, everything cleared.
        void reset() noexcept MOODZ_NONBLOCKING;

        // Take effect at the start of the next process call, the cutoff on the next step of the
        // modulation grid while the LFO has a depth; bounded.
        void setFactorIndex (int index) noexcept MOODZ_NONBLOCKING;
        void select (FilterChoice choice) noexcept MOODZ_NONBLOCKING;
        void setCutoffHz (float hz) noexcept MOODZ_NONBLOCKING;
        void setResonance (float amount) noexcept MOODZ_NONBLOCKING;
        void setDriveDb (float db) noexcept MOODZ_NONBLOCKING;
        void setDriveCurveIndex (int index) noexcept MOODZ_NONBLOCKING;
        void setLfoShapeIndex (int index) noexcept MOODZ_NONBLOCKING;
        void setLfoRateIndex (int index) noexcept MOODZ_NONBLOCKING;
        // Clamped to +/-maxLfoDepthOctaves; a non-finite value is ignored.
        void setLfoDepthOctaves (float octaves) noexcept MOODZ_NONBLOCKING;
        void setTempoBpm (double bpm) noexcept MOODZ_NONBLOCKING;
        // The song position in quarter notes, at the first sample of the next process call.
        void syncLfoToPosition (double quarterNotes) noexcept MOODZ_NONBLOCKING;

        // The cutoff after the LFO, updated on the modulation grid
        float modulatedCutoffHz() const noexcept;

        // One or two channels; processes at most as many samples as the shortest span holds. An input
        // sample beyond maxInputSample, NaN and infinities included, counts as 0: it is not a signal, and
        // it can neither leave the filter state non-finite nor come out as a click.
        void process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;

    private:
        struct Path
        {
            Oversampler oversampler;
            InputDrive drive;
            CrossfadingFilterBank filters;
            WholeSampleDelay delay;
        };

        bool lfoIsActive() const noexcept;
        void updateModulation() noexcept MOODZ_NONBLOCKING;
        void applyCutoff() noexcept MOODZ_NONBLOCKING;
        void startPendingCrossfade() noexcept MOODZ_NONBLOCKING;
        void startPath (std::size_t index) noexcept MOODZ_NONBLOCKING;
        void render (Path& path, std::span<std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;

        std::array<Path, Oversampler::factorCount> paths;
        // The outgoing path renders into this scratch, in chunks of its size
        std::array<std::array<float, Oversampler::chunkSize>, 2> scratch {};
        FilterChoice choice {};
        std::size_t pending = 0;
        std::size_t current = 0;
        std::size_t outgoing = 0;
        int crossfadeLength = 960;
        int crossfadePosition = 0; // samples done; crossfadeLength when no crossfade runs
        float cutoffHz = 1000.0f;
        float resonance = 0.0f;
        float driveDb = InputDrive::minDb;
        int driveCurveIndex = InputDrive::defaultCurveIndex;

        SyncedLfo lfo;
        int gridPosition = 0; // base samples since the last step of the modulation grid
        float slewCoefficient = 1.0f; // of the one-pole on the LFO value, per grid step
        float slewedValue = 0.0f;
        bool slewStartsOnNextProcess = true; // the song position is known only then
        float lfoDepthTarget = 0.0f;
        float lfoDepth = 0.0f;
        float lfoDepthStep = 0.0f;
        int lfoDepthStepsLeft = 0;
        int lfoDepthRampSteps = 1;
        float modulatedCutoff = 1000.0f;
    };
}
