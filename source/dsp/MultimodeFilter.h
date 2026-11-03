#pragma once

#include "dsp/Realtime.h"

#include <sst/filters++.h>

#include <span>
#include <string>

namespace moodz::dsp
{
    // A filter model of sst-filters and its configuration (passband, slope, drive, submodel).
    struct FilterSetup
    {
        sst::filtersplusplus::FilterModel model = sst::filtersplusplus::FilterModel::None;
        sst::filtersplusplus::ModelConfig config {};
        std::string name; // stable identifier, used in file names
        std::string label; // shown to the user
    };

    // One sst-filters model on linked stereo (the same coefficients on both channels, SPEC).
    // Processes in internal packets of packetSize samples, kept across host blocks, so the
    // output does not depend on how the host splits its blocks (.claude/rules/dsp.md).
    // Cutoff and resonance are read at the start of each packet; sst-filters interpolates its
    // coefficients across the packet.
    //
    // Precondition: the caller flushes denormals (FTZ/DAZ), as processBlock does with
    // juce::ScopedNoDenormals. sst-filters does not, and without it a decaying ladder can cost
    // tens of times more CPU (rt-safety review of task 0007).
    class MultimodeFilter
    {
    public:
        static constexpr int packetSize = 16;
        static constexpr float minCutoffHz = 20.0f;
        static constexpr float maxCutoffHz = 20000.0f;

        // Off the audio thread. Returns false if the setup is not a valid sst-filters
        // configuration or needs delay line memory (comb models, not supported).
        bool configure (const FilterSetup& setup);

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept MOODZ_NONBLOCKING;

        // Clamped to [minCutoffHz, maxCutoffHz]; non-finite ignored. In process, the cutoff is
        // also kept under 0.45 * sample rate, so it stays valid when prepare changes the rate.
        void setCutoffHz (float hz) noexcept MOODZ_NONBLOCKING;
        // Clamped to [0, 1]; non-finite ignored.
        void setResonance (float amount) noexcept MOODZ_NONBLOCKING;
        // The model's continuous setting, 0..1: OB-Xd 4-pole Morph, K35 continuous saturation
        // (sst-filters `extra` coefficient argument); ignored by other models. Clamped to [0, 1];
        // non-finite ignored.
        void setExtra (float amount) noexcept MOODZ_NONBLOCKING;

        // One or two channels; with one channel, the filter runs mono. Processes at most as many
        // samples as the shortest channel span holds.
        void process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;

    private:
        sst::filtersplusplus::Filter filter;
        FilterSetup setup;
        double sampleRate = 48000.0;
        float cutoffHz = 1000.0f;
        float nyquistGuardHz = 21600.0f; // 0.45 * sampleRate
        float resonance = 0.0f;
        float extra = 0.0f;
        int packetPosition = 0;
        bool ready = false;
    };
}
