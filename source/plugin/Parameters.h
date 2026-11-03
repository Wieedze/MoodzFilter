#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// The single place where parameters are declared (.claude/rules/juce.md).
// After the first release an ID is never renamed or removed; tests/plugin/GainOutTests.cpp
// freezes the list.
namespace moodz::parameters
{
    namespace id
    {
        inline constexpr const char* gainOut = "gainOut";
        inline constexpr const char* filterModel = "filterModel";
        inline constexpr const char* cutoff = "cutoff";
        inline constexpr const char* resonance = "resonance";
        inline constexpr const char* filterMode = "filterMode";
    }

    // Parameter version passed to juce::ParameterID, incremented with every addition:
    // 1 = gainOut (task 0003), 2 = filterModel, cutoff, resonance (task 0008),
    // 3 = filterMode (task 0009). A parameter keeps the version it was added with.
    inline constexpr int version = 3;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Display of gain values: "-inf" at the silence floor, otherwise "+6.0 dB", "-12.5 dB"
    juce::String decibelsToText (float db);
    float textToDecibels (const juce::String& text);

    // Display of frequencies: "440 Hz" below 1 kHz, "1.00 kHz" above
    juce::String hertzToText (float hz);
    float textToHertz (const juce::String& text);

    static_assert (std::atomic<float>::is_always_lock_free, "the audio thread reads parameters through std::atomic<float>");

    // Model and mode indices from the raw choice values, always within their lists: NaN,
    // infinity and out-of-range values are bounded in float before the conversion to int.
    int toModelIndex (float rawValue) noexcept;
    int toModeIndex (float rawValue) noexcept;

    // Plain copy of the parameter values, taken at the start of each block.
    // The dsp layer reads only this, never the APVTS.
    struct Params
    {
        float gainOutDb = 0.0f;
        int filterModel = 0;
        float cutoffHz = 1000.0f;
        float resonance = 0.2f; // 0..1, from the 0..100 % parameter
        int filterMode = 0; // index in dsp::filterModeLabels
    };

    // Raw atomic values, looked up once so that a snapshot needs no string lookup.
    class Snapshotter
    {
    public:
        explicit Snapshotter (const juce::AudioProcessorValueTreeState& state);
        Params snapshot() const noexcept;

    private:
        const std::atomic<float>* gainOut;
        const std::atomic<float>* filterModel;
        const std::atomic<float>* cutoff;
        const std::atomic<float>* resonance;
        const std::atomic<float>* filterMode;
    };
}
