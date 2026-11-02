#include "dsp/FilterCandidates.h"
#include "plugin/Parameters.h"
#include "plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

namespace
{
    template <typename ParameterType>
    ParameterType& parameter (PluginProcessor& processor, const juce::String& id)
    {
        for (auto* p : processor.getParameters())
            if (auto* typed = dynamic_cast<ParameterType*> (p); typed != nullptr && typed->paramID == id)
                return *typed;
        FAIL ("no parameter with ID " << id);
        throw std::logic_error ("unreachable");
    }

    juce::String textAt (juce::RangedAudioParameter& p, float value)
    {
        return static_cast<juce::AudioProcessorParameter&> (p).getText (p.convertTo0to1 (value), 32);
    }

    // Steady-state gain of the whole plugin on a stereo sine, in dB (RMS over the second half)
    double pluginGainDb (PluginProcessor& processor, double frequency)
    {
        constexpr int blockSize = 512;
        constexpr int length = 9728; // 19 blocks of 512
        processor.prepareToPlay (48000.0, blockSize);
        std::vector<float> input (length);
        for (int i = 0; i < length; ++i)
            input[static_cast<size_t> (i)] = 0.25f * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * i / 48000.0));

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        double inSum = 0.0, outSum = 0.0;
        for (int start = 0; start < length; start += blockSize)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (channel, i, input[static_cast<size_t> (start + i)]);
            processor.processBlock (buffer, midi);
            if (start >= length / 2)
                for (int i = 0; i < blockSize; ++i)
                {
                    const double in = input[static_cast<size_t> (start + i)];
                    const double out = buffer.getSample (0, i);
                    inSum += in * in;
                    outSum += out * out;
                }
        }
        return 10.0 * std::log10 (outSum / inSum);
    }
}

TEST_CASE ("Filter model parameter lists the candidates", "[plugin][filter]")
{
    PluginProcessor processor;
    auto& model = parameter<juce::AudioParameterChoice> (processor, "filterModel");
    const auto candidates = moodz::dsp::filterCandidates();
    REQUIRE (model.choices.size() == static_cast<int> (candidates.size()));
    for (size_t i = 0; i < candidates.size(); ++i)
        CHECK (model.choices[static_cast<int> (i)] == juce::String (candidates[i].label));
    CHECK (model.choices[0] == "Cytomic SVF");
    CHECK (model.getIndex() == 0);
}

TEST_CASE ("Cutoff and resonance match the SPEC", "[plugin][filter]")
{
    PluginProcessor processor;
    auto& cutoff = parameter<juce::AudioParameterFloat> (processor, "cutoff");
    CHECK (cutoff.range.start == Catch::Approx (20.0f));
    CHECK (cutoff.range.end == Catch::Approx (20000.0f));
    CHECK (cutoff.get() == Catch::Approx (1000.0f));
    // Logarithmic feel: the middle of the control is sqrt(20 * 20000) = 632.46 Hz
    CHECK (cutoff.convertFrom0to1 (0.5f) == Catch::Approx (632.46f).margin (1.0f));
    CHECK (textAt (cutoff, 440.0f) == "440 Hz");
    CHECK (textAt (cutoff, 1000.0f) == "1.00 kHz");

    auto& resonance = parameter<juce::AudioParameterFloat> (processor, "resonance");
    CHECK (resonance.range.start == Catch::Approx (0.0f));
    CHECK (resonance.range.end == Catch::Approx (100.0f));
    CHECK (resonance.get() == Catch::Approx (20.0f));
}

TEST_CASE ("The plugin filters with the selected model", "[plugin][filter]")
{
    // Cytomic SVF low pass at 1 kHz, resonance 0: a decade above is -42.8 dB in task 0007
    PluginProcessor processor;
    parameter<juce::AudioParameterFloat> (processor, "cutoff") = 1000.0f;
    parameter<juce::AudioParameterFloat> (processor, "resonance") = 0.0f;

    const auto above = pluginGainDb (processor, 10000.0);
    INFO ("10 kHz gain " << above << " dB");
    CHECK (above < -34.0);
    CHECK (above > -46.0);
    CHECK (pluginGainDb (processor, 100.0) == Catch::Approx (0.0).margin (1.0));
}

TEST_CASE ("Switching the model on every block stays finite", "[plugin][filter]")
{
    PluginProcessor processor;
    auto& model = parameter<juce::AudioParameterChoice> (processor, "filterModel");
    parameter<juce::AudioParameterFloat> (processor, "resonance") = 100.0f;
    processor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    juce::Random random (42);
    for (int block = 0; block < 375; ++block) // 2 s at 48 kHz
    {
        model = block % 7;
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
                buffer.setSample (channel, i, 0.25f * (random.nextFloat() * 2.0f - 1.0f));
        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
                REQUIRE (std::isfinite (buffer.getSample (channel, i)));
    }
}

TEST_CASE ("A state saved before the filter parameters loads with their defaults", "[plugin][state]")
{
    // The state written by tasks 0003 to 0007: only gainOut
    juce::XmlElement old ("MoodzFilter");
    old.setAttribute ("stateVersion", 1);
    auto* gain = old.createNewChildElement ("PARAM");
    gain->setAttribute ("id", "gainOut");
    gain->setAttribute ("value", -3.0);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (old, block);

    PluginProcessor processor;
    parameter<juce::AudioParameterFloat> (processor, "cutoff") = 5000.0f;
    processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK (parameter<juce::AudioParameterFloat> (processor, "gainOut").get() == Catch::Approx (-3.0f).margin (0.01));
    CHECK (parameter<juce::AudioParameterFloat> (processor, "cutoff").get() == Catch::Approx (1000.0f).margin (0.5));
    CHECK (parameter<juce::AudioParameterFloat> (processor, "resonance").get() == Catch::Approx (20.0f).margin (0.01));
    CHECK (parameter<juce::AudioParameterChoice> (processor, "filterModel").getIndex() == 0);
}

TEST_CASE ("The snapshot always gives a valid model index", "[plugin][filter]")
{
    // .claude/rules/juce.md: invariants applied in the snapshot. Converting NaN or an
    // out-of-range float to int is undefined behaviour, so the index is bounded in float first.
    using moodz::parameters::toModelIndex;
    CHECK (toModelIndex (0.0f) == 0);
    CHECK (toModelIndex (2.6f) == 3);
    CHECK (toModelIndex (6.0f) == 6);
    CHECK (toModelIndex (99.0f) == 6);
    CHECK (toModelIndex (-3.0f) == 0);
    CHECK (toModelIndex (std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK (toModelIndex (std::numeric_limits<float>::infinity()) == 0);
}

TEST_CASE ("Parameter versions follow the order of addition", "[plugin][parameters]")
{
    // .claude/rules/juce.md: gainOut shipped in version 1 (task 0003); the filter parameters
    // were added in version 2 (task 0008). A version hint never changes once set.
    PluginProcessor processor;
    CHECK (parameter<juce::AudioParameterFloat> (processor, "gainOut").getVersionHint() == 1);
    CHECK (parameter<juce::AudioParameterChoice> (processor, "filterModel").getVersionHint() == 2);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "cutoff").getVersionHint() == 2);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "resonance").getVersionHint() == 2);
}

namespace
{
    // The smallest processor that can own an APVTS built from createLayout(), to test the
    // snapshot on its own
    struct LayoutOnlyProcessor : juce::AudioProcessor
    {
        const juce::String getName() const override { return "layout"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return "Default"; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };
}

TEST_CASE ("The snapshot maps resonance from percent to 0..1", "[plugin][filter]")
{
    // SPEC: Resonance 0 to 100 %; the filter takes 0..1 (task 0008 Expected behaviour)
    LayoutOnlyProcessor owner;
    juce::AudioProcessorValueTreeState state { owner, nullptr, "MoodzFilter", moodz::parameters::createLayout() };
    const moodz::parameters::Snapshotter snapshotter { state };
    auto* resonance = state.getParameter ("resonance");

    for (auto [percent, expected] : { std::pair { 0.0f, 0.0f }, std::pair { 20.0f, 0.2f }, std::pair { 100.0f, 1.0f } })
    {
        resonance->setValueNotifyingHost (resonance->convertTo0to1 (percent));
        CHECK (snapshotter.snapshot().resonance == Catch::Approx (expected).margin (1e-6));
    }
}
