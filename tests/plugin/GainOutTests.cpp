#include "plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

namespace
{
    juce::AudioParameterFloat& floatParameter (PluginProcessor& processor, const juce::String& id)
    {
        for (auto* parameter : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioParameterFloat*> (parameter); withId != nullptr && withId->paramID == id)
                return *withId;
        FAIL ("no float parameter with ID " << id);
        throw std::logic_error ("unreachable");
    }

    juce::MemoryBlock saveState (PluginProcessor& processor)
    {
        juce::MemoryBlock block;
        processor.getStateInformation (block);
        return block;
    }
}

TEST_CASE ("Parameter IDs are frozen", "[plugin][parameters]")
{
    // .claude/rules/juce.md: an ID is never renamed or removed. Adding a parameter means
    // adding its ID here on purpose; a rename or removal makes this test fail.
    const std::set<std::string> expected { "gainOut" };

    PluginProcessor processor;
    std::set<std::string> actual;
    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            actual.insert (withId->paramID.toStdString());

    CHECK (actual == expected);
}

TEST_CASE ("Gain out matches the SPEC", "[plugin][parameters]")
{
    PluginProcessor processor;
    auto& gainOut = floatParameter (processor, "gainOut");

    CHECK (gainOut.range.start == Catch::Approx (-24.0f));
    CHECK (gainOut.range.end == Catch::Approx (24.0f));
    CHECK (gainOut.get() == Catch::Approx (0.0f).margin (1e-6));
}

TEST_CASE ("Gain out is applied to both channels", "[plugin][gain]")
{
    PluginProcessor processor;
    floatParameter (processor, "gainOut") = 6.0f;
    processor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    for (int channel = 0; channel < 2; ++channel)
        juce::FloatVectorOperations::fill (buffer.getWritePointer (channel), 0.25f, 256);
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    // 0.25 * 10^(6/20) = 0.498816 (Python 3: 0.25*10**(6/20))
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < 256; ++i)
            REQUIRE (buffer.getSample (channel, i) == Catch::Approx (0.498816f).margin (1e-5));
}

TEST_CASE ("A block of 0 samples is accepted", "[plugin][gain]")
{
    PluginProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 0);
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
    SUCCEED();
}

TEST_CASE ("State round trip", "[plugin][state]")
{
    PluginProcessor source;
    floatParameter (source, "gainOut") = -7.5f;
    const auto state = saveState (source);

    SECTION ("the saved state carries stateVersion 1")
    {
        const auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
        REQUIRE (xml != nullptr);
        CHECK (xml->getIntAttribute ("stateVersion", -1) == 1);
    }

    SECTION ("a new instance restores the value")
    {
        PluginProcessor destination;
        destination.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        CHECK (floatParameter (destination, "gainOut").get() == Catch::Approx (-7.5f).margin (0.01));
    }
}

TEST_CASE ("A state without stateVersion loads as version 1", "[plugin][state]")
{
    PluginProcessor source;
    floatParameter (source, "gainOut") = 3.0f;
    const auto state = saveState (source);

    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    REQUIRE (xml != nullptr);
    xml->removeAttribute ("stateVersion");
    juce::MemoryBlock withoutVersion;
    juce::AudioProcessor::copyXmlToBinary (*xml, withoutVersion);

    PluginProcessor destination;
    destination.setStateInformation (withoutVersion.getData(), static_cast<int> (withoutVersion.getSize()));
    CHECK (floatParameter (destination, "gainOut").get() == Catch::Approx (3.0f).margin (0.01));
}
