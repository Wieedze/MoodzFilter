#include "plugin/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace
{
    template <typename Parameter>
    Parameter& parameter (PluginProcessor& processor, const juce::String& id)
    {
        for (auto* p : processor.getParameters())
            if (auto* typed = dynamic_cast<Parameter*> (p); typed != nullptr && typed->paramID == id)
                return *typed;
        FAIL ("no parameter with ID " << id);
        throw std::logic_error ("unreachable");
    }

    void fillSine (juce::AudioBuffer<float>& buffer, int block)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (channel, i, 0.25f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 440.0 * (block * buffer.getNumSamples() + i) / 48000.0)));
    }
}

TEST_CASE ("One bad input sample leaves the plugin output finite", "[plugin][non-finite]")
{
    // Task 0014, FUTURE line from task 0006: Gain out at silence multiplied infinity into NaN, and the
    // filter kept any bad sample for good.
    for (float gainOutDb : { 0.0f, -60.0f })
        for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 3e38f })
        {
            INFO ("Gain out " << gainOutDb << ", bad sample " << bad);
            PluginProcessor processor;
            parameter<juce::AudioParameterFloat> (processor, "gainOut") = gainOutDb;
            processor.prepareToPlay (48000.0, 512);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            for (int block = 0; block < 40; ++block)
            {
                fillSine (buffer, block);
                if (block == 5)
                    buffer.setSample (0, 100, bad);
                processor.processBlock (buffer, midi);
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 512; ++i)
                        REQUIRE (std::isfinite (buffer.getSample (channel, i)));
            }
        }
}

TEST_CASE ("A bad sample on every block with every setting moving stays finite", "[plugin][non-finite]")
{
    // Task 0014, run under RealtimeSanitizer in CI. 2 s, a bad sample on every block, every setting
    // that picks a filter variant or the cutoff changing on every block, resonance 100 %.
    PluginProcessor processor;
    parameter<juce::AudioParameterFloat> (processor, "resonance") = 100.0f;
    auto& model = parameter<juce::AudioParameterChoice> (processor, "filterModel");
    auto& oversampling = parameter<juce::AudioParameterChoice> (processor, "oversampling");
    auto& drive = parameter<juce::AudioParameterFloat> (processor, "drive");
    auto& curve = parameter<juce::AudioParameterChoice> (processor, "driveCurve");
    auto& slope = parameter<juce::AudioParameterFloat> (processor, "slope");
    auto& saturation = parameter<juce::AudioParameterFloat> (processor, "k35Saturation");
    auto& variant = parameter<juce::AudioParameterChoice> (processor, "vintageVariant");
    auto& cutoff = parameter<juce::AudioParameterFloat> (processor, "cutoff");
    processor.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    const float bad[] { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -3e38f };
    for (int block = 0; block < 375; ++block)
    {
        model = block % 9;
        oversampling = block % 3;
        drive = static_cast<float> (block % 5) * 6.0f;
        curve = block % 2;
        slope = 6.0f + static_cast<float> (block % 7) * 3.0f;
        saturation = static_cast<float> (block % 5) * 25.0f;
        variant = block % 6;
        cutoff = block % 2 == 0 ? 20.0f : 20000.0f;
        fillSine (buffer, block);
        buffer.setSample (block % 2, block % 256, bad[block % 3]);
        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
                REQUIRE (std::isfinite (buffer.getSample (channel, i)));
    }
}
