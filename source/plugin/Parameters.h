#pragma once

#include "dsp/FilterModels.h"

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
        inline constexpr const char* slope = "slope";
        inline constexpr const char* k35Saturation = "k35Saturation";
        inline constexpr const char* vintageVariant = "vintageVariant";
    }

    // Parameter versions passed to juce::ParameterID, one per wave of additions; the next wave
    // gets the next number. A parameter keeps the version it was added with, forever.
    namespace version
    {
        inline constexpr int task0003 = 1; // gainOut
        inline constexpr int task0008 = 2; // filterModel, cutoff, resonance
        inline constexpr int task0010 = 3; // slope, k35Saturation, vintageVariant
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Display of gain values: "-inf" at the silence floor, otherwise "+6.0 dB", "-12.5 dB"
    juce::String decibelsToText (float db);
    float textToDecibels (const juce::String& text);

    // Display of frequencies: "440 Hz" below 1 kHz, "1.00 kHz" above
    juce::String hertzToText (float hz);
    float textToHertz (const juce::String& text);

    // Display of the slope: "24 dB/oct", "12.5 dB/oct"
    juce::String slopeToText (float db);

    static_assert (std::atomic<float>::is_always_lock_free, "the audio thread reads parameters through std::atomic<float>");

    // Model index from the raw choice value, always within the filter list: NaN, infinity
    // and out-of-range values are bounded in float before the conversion to int.
    int toModelIndex (float rawValue) noexcept;
    // Same for the Vintage variant list
    int toVintageVariantIndex (float rawValue) noexcept;

    // Plain copy of the parameter values, taken at the start of each block.
    // The dsp layer reads only this, never the APVTS.
    struct Params
    {
        float gainOutDb = 0.0f;
        int filterModel = 0;
        float cutoffHz = 1000.0f;
        float resonance = 0.2f; // 0..1, from the 0..100 % parameter
        float slopeDb = dsp::maxSlopeDb;
        float k35Saturation = dsp::defaultK35Saturation; // 0..1, from the 0..100 % parameter
        int vintageVariant = dsp::defaultVintageVariant;
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
        const std::atomic<float>* slope;
        const std::atomic<float>* k35Saturation;
        const std::atomic<float>* vintageVariant;
    };
}
