#include "plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <vector>

namespace
{
    juce::AudioParameterChoice& oversampling (PluginProcessor& processor)
    {
        for (auto* p : processor.getParameters())
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p); choice != nullptr && choice->paramID == "oversampling")
                return *choice;
        FAIL ("no oversampling parameter");
        throw std::logic_error ("unreachable");
    }

    juce::AudioParameterFloat& cutoff (PluginProcessor& processor)
    {
        for (auto* p : processor.getParameters())
            if (auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (p); parameter != nullptr && parameter->paramID == "cutoff")
                return *parameter;
        FAIL ("no cutoff parameter");
        throw std::logic_error ("unreachable");
    }
}

TEST_CASE ("Oversampling matches the SPEC", "[plugin][oversampling]")
{
    // SPEC: Off, 2x, 4x, default 2x. Added in version 4 (task 0012).
    PluginProcessor processor;
    auto& parameter = oversampling (processor);
    CHECK (parameter.choices == juce::StringArray { "Off", "2x", "4x" });
    CHECK (parameter.getIndex() == 1);
    CHECK (parameter.getVersionHint() == 4);
}

TEST_CASE ("The snapshot always gives a valid oversampling index", "[plugin][oversampling]")
{
    using moodz::parameters::toOversamplingIndex;
    CHECK (toOversamplingIndex (0.0f) == 0);
    CHECK (toOversamplingIndex (2.0f) == 2);
    CHECK (toOversamplingIndex (-3.0f) == 0);
    CHECK (toOversamplingIndex (99.0f) == 2);
    CHECK (toOversamplingIndex (std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK (toOversamplingIndex (std::numeric_limits<float>::infinity()) == 0);
}

TEST_CASE ("The reported latency is the same at every setting and rate", "[plugin][oversampling]")
{
    // ADR 0009: one constant latency, 40 samples (32 + 16 / 2, source/dsp/Oversampler.cpp)
    std::set<int> reported;
    for (int index : { 0, 1, 2 })
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            PluginProcessor processor;
            oversampling (processor) = index;
            processor.prepareToPlay (rate, 512);
            reported.insert (processor.getLatencySamples());
        }
    CHECK (reported == std::set<int> { 40 });

    // Known before the host prepares the plugin too
    PluginProcessor processor;
    CHECK (processor.getLatencySamples() == 40);
}

TEST_CASE ("The plugin output comes at the reported latency", "[plugin][oversampling]")
{
    // Impulse on both channels, Cytomic SVF Low-pass at 20 kHz: the peak at the latency ± 1 sample
    for (int index : { 0, 1, 2 })
    {
        PluginProcessor processor;
        oversampling (processor) = index;
        cutoff (processor) = 20000.0f;
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        buffer.clear();
        buffer.setSample (0, 100, 1.0f);
        buffer.setSample (1, 100, 1.0f);
        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto* samples = buffer.getReadPointer (channel);
            const auto peak = std::max_element (samples, samples + 512, [] (float a, float b) { return std::abs (a) < std::abs (b); }) - samples;
            INFO ("setting " << index << ", channel " << channel << ", peak at " << peak);
            CHECK (std::abs (peak - (100 + processor.getLatencySamples())) <= 1);
        }
    }
}

TEST_CASE ("Bypass delays the input by the reported latency", "[plugin][oversampling]")
{
    // playbook docs/07_REUSE.md: an uncompensated bypass shifts the track. Across two blocks.
    PluginProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    const int latency = processor.getLatencySamples();
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    std::vector<float> left, right;
    for (int block = 0; block < 2; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            buffer.setSample (0, i, static_cast<float> (block * 512 + i + 1));
            buffer.setSample (1, i, -static_cast<float> (block * 512 + i + 1));
        }
        processor.processBlockBypassed (buffer, midi);
        for (int i = 0; i < 512; ++i)
        {
            left.push_back (buffer.getSample (0, i));
            right.push_back (buffer.getSample (1, i));
        }
    }
    for (int n = 0; n < 1024; ++n)
    {
        const float expected = n < latency ? 0.0f : static_cast<float> (n - latency + 1);
        REQUIRE (left[static_cast<size_t> (n)] == Catch::Approx (expected).margin (1e-3));
        REQUIRE (right[static_cast<size_t> (n)] == Catch::Approx (-expected).margin (1e-3));
    }
}

TEST_CASE ("A state saved before the oversampling loads with its default", "[plugin][oversampling][state]")
{
    // .claude/rules/juce.md: the missing value falls back to the default, 2x; others are kept
    juce::XmlElement xml ("MoodzFilter");
    xml.setAttribute ("stateVersion", 1);
    auto* saved = xml.createNewChildElement ("PARAM");
    saved->setAttribute ("id", "cutoff");
    saved->setAttribute ("value", 5000.0);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (xml, block);

    PluginProcessor processor;
    oversampling (processor) = 2;
    processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK (oversampling (processor).getIndex() == 1);
    CHECK (cutoff (processor).get() == Catch::Approx (5000.0f).margin (0.01));
}
