#include "plugin/Parameters.h"
#include "plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
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

TEST_CASE ("Filter model parameter lists every model and passband in the frozen order", "[plugin][filter]")
{
    // ADR 0007: one entry per model and passband it has; the order is frozen, entries can only
    // be added at the end
    PluginProcessor processor;
    auto& model = parameter<juce::AudioParameterChoice> (processor, "filterModel");
    CHECK (model.choices
           == juce::StringArray { "Cytomic SVF Low-pass",
               "Cytomic SVF High-pass",
               "Cytomic SVF Band-pass",
               "Cytomic SVF Notch",
               "Vintage Ladder Low-pass",
               "OB-Xd 4-pole Low-pass",
               "Diode Ladder Low-pass",
               "K35 Low-pass",
               "K35 High-pass" });
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

TEST_CASE ("Switching the model and its settings on every block stays finite", "[plugin][filter]")
{
    // Every Model entry, Slope 6 to 24 dB in 3 dB steps (every Diode step and Morph values between),
    // K35 saturation 0 to 100 %, every Vintage variant, every Oversampling setting, Drive 0 to 24 dB, both Drive curves, changed on every block
    PluginProcessor processor;
    auto& model = parameter<juce::AudioParameterChoice> (processor, "filterModel");
    auto& slope = parameter<juce::AudioParameterFloat> (processor, "slope");
    auto& saturation = parameter<juce::AudioParameterFloat> (processor, "k35Saturation");
    auto& variant = parameter<juce::AudioParameterChoice> (processor, "vintageVariant");
    auto& oversampling = parameter<juce::AudioParameterChoice> (processor, "oversampling");
    auto& drive = parameter<juce::AudioParameterFloat> (processor, "drive");
    auto& driveCurve = parameter<juce::AudioParameterChoice> (processor, "driveCurve");
    parameter<juce::AudioParameterFloat> (processor, "resonance") = 100.0f;
    processor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    juce::Random random (42);
    for (int block = 0; block < 375; ++block) // 2 s at 48 kHz
    {
        model = block % 9;
        slope = 6.0f + static_cast<float> (block % 7) * 3.0f;
        saturation = static_cast<float> (block % 5) * 25.0f;
        variant = block % 6;
        oversampling = block % 3;
        drive = static_cast<float> (block % 5) * 6.0f;
        driveCurve = block % 2;
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
    CHECK (toModelIndex (8.0f) == 8);
    CHECK (toModelIndex (9.0f) == 8);
    CHECK (toModelIndex (99.0f) == 8);
    CHECK (toModelIndex (-3.0f) == 0);
    CHECK (toModelIndex (std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK (toModelIndex (std::numeric_limits<float>::infinity()) == 0);
    CHECK (toModelIndex (-std::numeric_limits<float>::infinity()) == 0);
}

TEST_CASE ("Parameter versions follow the order of addition", "[plugin][parameters]")
{
    // .claude/rules/juce.md: gainOut shipped in version 1 (task 0003); the filter parameters
    // were added in version 2 (task 0008), the per-model settings in version 3 (task 0010).
    // A version hint never changes once set.
    PluginProcessor processor;
    CHECK (parameter<juce::AudioParameterFloat> (processor, "gainOut").getVersionHint() == 1);
    CHECK (parameter<juce::AudioParameterChoice> (processor, "filterModel").getVersionHint() == 2);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "cutoff").getVersionHint() == 2);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "resonance").getVersionHint() == 2);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "slope").getVersionHint() == 3);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "k35Saturation").getVersionHint() == 3);
    CHECK (parameter<juce::AudioParameterChoice> (processor, "vintageVariant").getVersionHint() == 3);
    CHECK (parameter<juce::AudioParameterChoice> (processor, "oversampling").getVersionHint() == 4);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "drive").getVersionHint() == 5);
    CHECK (parameter<juce::AudioParameterChoice> (processor, "driveCurve").getVersionHint() == 5);
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

namespace
{
    PluginProcessor& withFilter (PluginProcessor& processor, int model)
    {
        parameter<juce::AudioParameterChoice> (processor, "filterModel") = model;
        parameter<juce::AudioParameterFloat> (processor, "cutoff") = 1000.0f;
        parameter<juce::AudioParameterFloat> (processor, "resonance") = 0.0f;
        return processor;
    }

    // Model entries, ADR 0007
    constexpr int cytomicHighPass = 1, cytomicBandPass = 2, cytomicNotch = 3, k35HighPass = 8;
}

TEST_CASE ("Cytomic SVF high pass", "[plugin][filter]")
{
    // Task 0007 measured the Cytomic SVF high pass at 1 kHz: 100 Hz at -40.1 dB
    PluginProcessor processor;
    withFilter (processor, cytomicHighPass);
    const auto below = pluginGainDb (processor, 100.0);
    INFO ("100 Hz gain " << below << " dB");
    CHECK (below < -34.0);
    CHECK (below > -46.0);
    CHECK (pluginGainDb (processor, 10000.0) == Catch::Approx (0.0).margin (1.0));
}

TEST_CASE ("Cytomic SVF band pass and notch", "[plugin][filter]")
{
    // Measured on this signal: band pass 1 kHz 14.1 dB over 100 Hz and 15.4 dB over 10 kHz;
    // notch 1 kHz at -122.5 dB, 100 Hz and 10 kHz within 0.2 dB of unity. Printed by:
    // ./Builds/PluginTests "Cytomic SVF band pass and notch" -s | grep -E "band pass|notch 1"
    PluginProcessor bandProcessor, notchProcessor;
    withFilter (bandProcessor, cytomicBandPass);
    withFilter (notchProcessor, cytomicNotch);
    const auto band100 = pluginGainDb (bandProcessor, 100.0), band1k = pluginGainDb (bandProcessor, 1000.0), band10k = pluginGainDb (bandProcessor, 10000.0);
    const auto notch100 = pluginGainDb (notchProcessor, 100.0), notch1k = pluginGainDb (notchProcessor, 1000.0), notch10k = pluginGainDb (notchProcessor, 10000.0);
    INFO ("band pass 100 Hz " << band100 << " dB, 1 kHz " << band1k << " dB, 10 kHz " << band10k << " dB");
    INFO ("notch 100 Hz " << notch100 << " dB, 1 kHz " << notch1k << " dB, 10 kHz " << notch10k << " dB");
    CHECK (band1k - band100 > 12.0);
    CHECK (band1k - band10k > 12.0);
    CHECK (notch100 - notch1k > 60.0);
    CHECK (notch10k - notch1k > 60.0);
}

TEST_CASE ("K35 high pass", "[plugin][filter]")
{
    // Measured on this signal: 10 kHz 20.1 dB over 100 Hz. Printed by:
    // ./Builds/PluginTests "K35 high pass" -s | grep "K35 high pass 100"
    PluginProcessor processor;
    withFilter (processor, k35HighPass);
    const auto below = pluginGainDb (processor, 100.0), above = pluginGainDb (processor, 10000.0);
    INFO ("K35 high pass 100 Hz " << below << " dB, 10 kHz " << above << " dB");
    CHECK (above - below > 16.0);
}

namespace
{
    juce::MemoryBlock stateWith (std::initializer_list<std::pair<const char*, double>> values)
    {
        juce::XmlElement xml ("MoodzFilter");
        xml.setAttribute ("stateVersion", 1);
        for (auto [id, value] : values)
        {
            auto* p = xml.createNewChildElement ("PARAM");
            p->setAttribute ("id", id);
            p->setAttribute ("value", value);
        }
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);
        return block;
    }

    void checkLoadedValues (PluginProcessor& processor)
    {
        CHECK (parameter<juce::AudioParameterChoice> (processor, "filterModel").getIndex() == 3);
        CHECK (parameter<juce::AudioParameterFloat> (processor, "gainOut").get() == Catch::Approx (-3.0f).margin (0.01));
        CHECK (parameter<juce::AudioParameterFloat> (processor, "cutoff").get() == Catch::Approx (5000.0f).margin (0.5));
        CHECK (parameter<juce::AudioParameterFloat> (processor, "resonance").get() == Catch::Approx (60.0f).margin (0.01));
    }
}

TEST_CASE ("A state saved by task 0008 loads with every value kept", "[plugin][state]")
{
    // The state written by task 0008: gainOut, filterModel, cutoff, resonance
    const auto block = stateWith ({ { "gainOut", -3.0 }, { "filterModel", 3.0 }, { "cutoff", 5000.0 }, { "resonance", 60.0 } });
    PluginProcessor processor;
    processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    checkLoadedValues (processor);
}

TEST_CASE ("A state with the removed filterMode parameter loads", "[plugin][state]")
{
    // The first build of task 0009 (8c85445) saved filterMode, removed by ADR 0007: ignored
    const auto block = stateWith ({ { "gainOut", -3.0 }, { "filterModel", 3.0 }, { "cutoff", 5000.0 }, { "resonance", 60.0 }, { "filterMode", 2.0 } });
    PluginProcessor processor;
    processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    checkLoadedValues (processor);
}

TEST_CASE ("The per-model settings match ADR 0008", "[plugin][filter]")
{
    PluginProcessor processor;
    auto& slope = parameter<juce::AudioParameterFloat> (processor, "slope");
    CHECK (slope.range.start == Catch::Approx (6.0f));
    CHECK (slope.range.end == Catch::Approx (24.0f));
    CHECK (slope.get() == Catch::Approx (24.0f));
    CHECK (textAt (slope, 24.0f) == "24 dB/oct");
    CHECK (textAt (slope, 12.5f) == "12.5 dB/oct");

    auto& saturation = parameter<juce::AudioParameterFloat> (processor, "k35Saturation");
    CHECK (saturation.range.start == Catch::Approx (0.0f));
    CHECK (saturation.range.end == Catch::Approx (100.0f));
    CHECK (saturation.get() == Catch::Approx (25.0f));

    auto& variant = parameter<juce::AudioParameterChoice> (processor, "vintageVariant");
    CHECK (variant.choices
           == juce::StringArray { "Runge-Kutta",
               "Runge-Kutta compensated",
               "Huovilainen",
               "Huovilainen compensated",
               "Huovilainen 2010",
               "Huovilainen 2010 compensated" });
    CHECK (variant.getIndex() == 1);
}

TEST_CASE ("The snapshot carries the per-model settings", "[plugin][filter]")
{
    // The filter takes the K35 saturation as 0..1, from the 0..100 % parameter
    LayoutOnlyProcessor owner;
    juce::AudioProcessorValueTreeState state { owner, nullptr, "MoodzFilter", moodz::parameters::createLayout() };
    const moodz::parameters::Snapshotter snapshotter { state };
    auto set = [&] (const char* id, float value) {
        auto* p = state.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    set ("slope", 15.0f);
    set ("k35Saturation", 70.0f);
    set ("vintageVariant", 4.0f);
    const auto params = snapshotter.snapshot();
    CHECK (params.slopeDb == Catch::Approx (15.0f).margin (0.01));
    CHECK (params.k35Saturation == Catch::Approx (0.7f).margin (1e-4));
    CHECK (params.vintageVariant == 4);
}

TEST_CASE ("The snapshot always gives a valid Vintage variant index", "[plugin][filter]")
{
    using moodz::parameters::toVintageVariantIndex;
    CHECK (toVintageVariantIndex (0.0f) == 0);
    CHECK (toVintageVariantIndex (5.0f) == 5);
    CHECK (toVintageVariantIndex (6.0f) == 5);
    CHECK (toVintageVariantIndex (-1.0f) == 0);
    CHECK (toVintageVariantIndex (std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK (toVintageVariantIndex (std::numeric_limits<float>::infinity()) == 0);
}

namespace
{
    constexpr int obxd = 5, diodeLadder = 6, k35LowPass = 7;

    // Bounds set from the measurements printed by the tests below (commands in each test)
    // Diode Ladder at 4 kHz: -30.8, -41.8, -54.4, -66.8 dB for 6, 12, 18, 24 dB, smallest step 11.0 dB
    constexpr double minimumStepDb = 8.0;
    // OB-Xd at 4 kHz: -16.9, -29.4, -34.7, -41.9, -54.4 dB for 6, 12, 15, 18, 24 dB, smallest gap 5.3 dB
    constexpr double minimumMorphStepDb = 3.0;
    // K35 third harmonic: -55.9 dB at 0 %, -9.3 dB at 100 %, a rise of 46.6 dB
    constexpr double minimumHarmonicRiseDb = 30.0;

    double gainAtSlope (int model, float slopeDb, double frequency)
    {
        PluginProcessor processor;
        withFilter (processor, model);
        parameter<juce::AudioParameterFloat> (processor, "slope") = slopeDb;
        return pluginGainDb (processor, frequency);
    }
}

TEST_CASE ("Slope steepens the Diode Ladder step by step", "[plugin][filter]")
{
    // Sine at 4 kHz, two octaves above the 1 kHz cutoff: each 6 dB step attenuates more.
    // Gains printed by: ./Builds/PluginTests "Slope steepens the Diode Ladder step by step" -s | grep "Diode slope"
    const std::array<float, 4> slopes { 6.0f, 12.0f, 18.0f, 24.0f };
    std::array<double, 4> gains {};
    for (size_t i = 0; i < slopes.size(); ++i)
    {
        gains[i] = gainAtSlope (diodeLadder, slopes[i], 4000.0);
        SUCCEED ("Diode slope " << slopes[i] << " dB: " << gains[i] << " dB at 4 kHz");
    }
    for (size_t i = 1; i < gains.size(); ++i)
        CHECK (gains[i - 1] - gains[i] > minimumStepDb);
    // Between steps, the nearest one plays: 13 dB sounds like 12 dB
    CHECK (gainAtSlope (diodeLadder, 13.0f, 4000.0) == Catch::Approx (gains[1]).margin (0.01));
}

TEST_CASE ("Slope morphs the OB-Xd 4-pole continuously", "[plugin][filter]")
{
    // Sine at 4 kHz, two octaves above the 1 kHz cutoff: attenuation grows with the slope, and a
    // slope between two steps sits between them.
    // Gains printed by: ./Builds/PluginTests "Slope morphs the OB-Xd 4-pole continuously" -s | grep "OB-Xd slope"
    const std::array<float, 5> slopes { 6.0f, 12.0f, 15.0f, 18.0f, 24.0f };
    std::array<double, 5> gains {};
    for (size_t i = 0; i < slopes.size(); ++i)
    {
        gains[i] = gainAtSlope (obxd, slopes[i], 4000.0);
        SUCCEED ("OB-Xd slope " << slopes[i] << " dB: " << gains[i] << " dB at 4 kHz");
    }
    for (size_t i = 1; i < gains.size(); ++i)
        CHECK (gains[i - 1] - gains[i] > minimumMorphStepDb);
}

namespace
{
    // Level in dB (re full scale) of one frequency in the second half of the output, single DFT bin
    double harmonicLevelDb (PluginProcessor& processor, double fundamental, double frequency)
    {
        constexpr int blockSize = 512;
        constexpr int length = 48128; // 94 blocks of 512, about 1 s
        processor.prepareToPlay (48000.0, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        double re = 0.0, im = 0.0;
        int count = 0;
        for (int start = 0; start + blockSize <= length; start += blockSize)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (channel, i, static_cast<float> (std::sin (2.0 * std::numbers::pi * fundamental * (start + i) / 48000.0)));
            processor.processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
                if (const int n = start + i; n >= length / 2)
                {
                    const double phase = 2.0 * std::numbers::pi * frequency * n / 48000.0;
                    re += buffer.getSample (0, i) * std::cos (phase);
                    im += buffer.getSample (0, i) * std::sin (phase);
                    ++count;
                }
        }
        return 20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / count + 1e-12);
    }
}

TEST_CASE ("K35 saturation adds harmonics", "[plugin][filter]")
{
    // K35 Low-pass, cutoff 1 kHz, resonance 50 %, sine at 200 Hz, 0 dBFS: the tanh stage
    // (sst-filters K35Filter.h) adds odd harmonics. Third harmonic (600 Hz) at 0 and 100 %
    // printed by: ./Builds/PluginTests "K35 saturation adds harmonics" -s | grep "third harmonic"
    auto thirdHarmonic = [] (float saturation) {
        PluginProcessor processor;
        withFilter (processor, k35LowPass);
        parameter<juce::AudioParameterFloat> (processor, "resonance") = 50.0f;
        parameter<juce::AudioParameterFloat> (processor, "k35Saturation") = saturation;
        return harmonicLevelDb (processor, 200.0, 600.0);
    };
    const auto clean = thirdHarmonic (0.0f), driven = thirdHarmonic (100.0f);
    SUCCEED ("third harmonic at 0 %: " << clean << " dB, at 100 %: " << driven << " dB");
    CHECK (driven - clean > minimumHarmonicRiseDb);
}

TEST_CASE ("A state saved by task 0009 loads with the default settings", "[plugin][state]")
{
    // Task 0009 saved gainOut, filterModel, cutoff, resonance: the settings take their defaults
    const auto block = stateWith ({ { "gainOut", -3.0 }, { "filterModel", 3.0 }, { "cutoff", 5000.0 }, { "resonance", 60.0 } });
    PluginProcessor processor;
    parameter<juce::AudioParameterFloat> (processor, "slope") = 6.0f;
    parameter<juce::AudioParameterFloat> (processor, "k35Saturation") = 90.0f;
    parameter<juce::AudioParameterChoice> (processor, "vintageVariant") = 3;
    processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    checkLoadedValues (processor);
    CHECK (parameter<juce::AudioParameterFloat> (processor, "slope").get() == Catch::Approx (24.0f).margin (0.01));
    CHECK (parameter<juce::AudioParameterFloat> (processor, "k35Saturation").get() == Catch::Approx (25.0f).margin (0.01));
    CHECK (parameter<juce::AudioParameterChoice> (processor, "vintageVariant").getIndex() == 1);
}
