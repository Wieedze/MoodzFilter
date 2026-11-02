#pragma once

#include "core/ToolsCore.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <span>
#include <string>
#include <vector>

namespace moodz::tools
{
    // A parameter setup rendered for every test signal. When automated, Gain out jumps from
    // startDb to endDb at the middle of the signal, exactly on that sample.
    struct RenderCase
    {
        std::string name;
        float startDb = 0.0f;
        float endDb = 0.0f;
        bool automated = false;
    };

    struct TestSignal
    {
        std::string name;
        std::vector<float> samples;
    };

    // The fixed set of cases and signals behind `render`, `check` and the golden references.
    // Adding one changes the renders: new references go through a `golden:` commit.
    std::vector<RenderCase> renderCases();
    std::vector<TestSignal> testSignals();

    std::string renderFileName (const RenderCase& renderCase, const TestSignal& signal);

    // Runs the mono input on both channels of a fresh PluginProcessor at renderSampleRate,
    // in blocks of blockSize samples (split at the automation point), and returns the stereo output.
    juce::AudioBuffer<float> renderThroughPlugin (const RenderCase& renderCase, std::span<const float> input, int blockSize);

    // Block size used for the references.
    inline constexpr int referenceBlockSize = 512;
}
