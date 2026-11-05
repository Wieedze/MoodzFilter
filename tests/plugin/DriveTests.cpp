#include "plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>

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
}

TEST_CASE ("Drive and Drive curve match the SPEC", "[plugin][drive]")
{
    // SPEC, ADR 0009: Drive 0 to 24 dB, default 0 dB; Symmetric, Asymmetric, default Symmetric
    PluginProcessor processor;
    auto& drive = parameter<juce::AudioParameterFloat> (processor, "drive");
    CHECK (drive.range.start == Catch::Approx (0.0f));
    CHECK (drive.range.end == Catch::Approx (24.0f));
    CHECK (drive.get() == Catch::Approx (0.0f));
    CHECK (static_cast<juce::AudioProcessorParameter&> (drive).getText (drive.convertTo0to1 (12.0f), 32) == "+12.0 dB");
    auto& curve = parameter<juce::AudioParameterChoice> (processor, "driveCurve");
    CHECK (curve.choices == juce::StringArray { "Symmetric", "Asymmetric" });
    CHECK (curve.getIndex() == 0);
}

TEST_CASE ("The snapshot always gives a valid Drive curve index", "[plugin][drive]")
{
    using moodz::parameters::toDriveCurveIndex;
    CHECK (toDriveCurveIndex (0.0f) == 0);
    CHECK (toDriveCurveIndex (1.0f) == 1);
    CHECK (toDriveCurveIndex (-3.0f) == 0);
    CHECK (toDriveCurveIndex (99.0f) == 1);
    CHECK (toDriveCurveIndex (std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK (toDriveCurveIndex (std::numeric_limits<float>::infinity()) == 0);
}

TEST_CASE ("Drive reaches the plugin output, already set when playback starts", "[plugin][drive]")
{
    // 1 kHz sine at -24 dBFS, cutoff 20 kHz, resonance 0, Off, Gain out 0 dB. Saved at 24 dB:
    // the very first samples after the latency are already driven, no ramp from 0 dB. The tanh of
    // 15.85 x 0.0625 peaks at 0.757 (python3 -c "from math import tanh; print(tanh(10**(24/20)/16))").
    PluginProcessor processor;
    parameter<juce::AudioParameterFloat> (processor, "cutoff") = 20000.0f;
    parameter<juce::AudioParameterFloat> (processor, "resonance") = 0.0f;
    parameter<juce::AudioParameterChoice> (processor, "oversampling") = 0;
    parameter<juce::AudioParameterFloat> (processor, "drive") = 24.0f;
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < 512; ++i)
            buffer.setSample (channel, i, 0.0625f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 1000.0 * i / 48000.0)));
    processor.processBlock (buffer, midi);
    // First peak of the sine: sample 12, out at 12 + 40. Cutoff 20 kHz takes little at 1 kHz.
    CHECK (std::abs (buffer.getSample (0, 52)) == Catch::Approx (0.757f).margin (0.02f));
    CHECK (processor.getLatencySamples() == 40);
}

TEST_CASE ("A state saved before the drive loads with its defaults", "[plugin][drive][state]")
{
    // .claude/rules/juce.md: the missing values fall back to 0 dB and Symmetric; others are kept
    juce::XmlElement xml ("MoodzFilter");
    xml.setAttribute ("stateVersion", 1);
    auto* saved = xml.createNewChildElement ("PARAM");
    saved->setAttribute ("id", "cutoff");
    saved->setAttribute ("value", 5000.0);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (xml, block);

    PluginProcessor processor;
    parameter<juce::AudioParameterFloat> (processor, "drive") = 12.0f;
    parameter<juce::AudioParameterChoice> (processor, "driveCurve") = 1;
    processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK (parameter<juce::AudioParameterFloat> (processor, "drive").get() == Catch::Approx (0.0f));
    CHECK (parameter<juce::AudioParameterChoice> (processor, "driveCurve").getIndex() == 0);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "cutoff").get() == Catch::Approx (5000.0f).margin (0.01));
}
