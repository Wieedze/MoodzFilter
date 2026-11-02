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
    }

    // Parameter version passed to juce::ParameterID, incremented with every addition.
    inline constexpr int version = 1;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Plain copy of the parameter values, taken at the start of each block.
    // The dsp layer reads only this, never the APVTS.
    struct Params
    {
        float gainOutDb = 0.0f;
    };

    // Raw atomic values, looked up once so that a snapshot needs no string lookup.
    class Snapshotter
    {
    public:
        explicit Snapshotter (const juce::AudioProcessorValueTreeState& state);
        Params snapshot() const noexcept;

    private:
        const std::atomic<float>* gainOut;
    };
}
