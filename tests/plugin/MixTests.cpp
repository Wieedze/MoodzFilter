#include "plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

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

    juce::AudioParameterFloat& floatParameter (PluginProcessor& p, const juce::String& id) { return parameter<juce::AudioParameterFloat> (p, id); }

    constexpr int blockSize = 512;

    // Left output of a sine at 48 kHz, stereo. onBlock moves the parameters before each block.
    std::vector<float> render (PluginProcessor& processor, int total, double frequency, float peak, const std::function<void (int)>& onBlock = {})
    {
        processor.prepareToPlay (48000.0, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        std::vector<float> output;
        for (int start = 0; start < total; start += blockSize)
        {
            if (onBlock)
                onBlock (start);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (channel, i, peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * (start + i) / 48000.0)));
            processor.processBlock (buffer, midi);
            output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
        }
        return output;
    }

    float largestDifference (const std::vector<float>& a, const std::vector<float>& b, std::size_t from, std::size_t to)
    {
        float largest = 0.0f;
        for (std::size_t i = from; i < to; ++i)
        {
            const float difference = std::abs (a[i] - b[i]);
            largest = std::isfinite (difference) ? std::max (largest, difference) : 1e30f;
        }
        return largest;
    }

    // Click measure of task 0011, from the given sample to the end
    double clickFrom (const std::vector<float>& y, std::size_t from)
    {
        double largest = 0.0;
        for (std::size_t n = std::max<std::size_t> (from, 2); n < y.size(); ++n)
            largest = std::max (largest, std::abs (static_cast<double> (y[n]) - 2.0 * y[n - 1] + y[n - 2]));
        return largest;
    }

    // Bump measure of task 0013: 200 Hz has a period of 240 samples, which averages it out
    double bumpFrom (const std::vector<float>& y, std::size_t from, std::size_t to)
    {
        double largest = 0.0;
        for (std::size_t start = from; start < to; ++start)
        {
            double sum = 0.0;
            for (std::size_t i = 0; i < 240; ++i)
                sum += static_cast<double> (y[start + i]);
            largest = std::max (largest, std::abs (sum / 240.0));
        }
        return largest;
    }
}

TEST_CASE ("Mix matches the SPEC", "[plugin][mix]")
{
    // SPEC: 0 to 100 %, default 100 %
    PluginProcessor processor;
    auto& mix = floatParameter (processor, "mix");
    CHECK (mix.range.start == Catch::Approx (0.0f));
    CHECK (mix.range.end == Catch::Approx (100.0f));
    CHECK (mix.get() == Catch::Approx (100.0f));
    CHECK (mix.getLabel() == "%");
}

TEST_CASE ("At Mix 0 % the plugin gives the input delayed, like bypass", "[plugin][mix]")
{
    // SPEC, ADR 0009, Gain out at 0 dB: whatever Drive does, the dry path is the input 40 samples later,
    // in a session that opens at Mix 0 %, so with no ramp from 100 %.
    PluginProcessor processor;
    floatParameter (processor, "mix") = 0.0f;
    floatParameter (processor, "drive") = 24.0f;
    floatParameter (processor, "cutoff") = 200.0f;
    const auto output = render (processor, 9600, 1000.0, 0.25f);
    CHECK (processor.getLatencySamples() == 40);
    for (std::size_t i = 40; i < output.size(); ++i)
    {
        const float expected = 0.25f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 1000.0 * static_cast<double> (i - 40) / 48000.0));
        REQUIRE (std::abs (output[i] - expected) < 1e-5f);
    }
}

TEST_CASE ("A Mix change during playback reaches the output", "[plugin][mix]")
{
    // Post-mortem 0013: a bound alone does not prove it arrives. 100 % to 25 % at block 20: unchanged before,
    // different after, and equal at the end to a render at 25 % from the start.
    auto withMix = [] (const std::function<void (PluginProcessor&, int)>& onBlock) {
        PluginProcessor processor;
        floatParameter (processor, "cutoff") = 200.0f;
        return render (processor, 19200, 1000.0, 0.25f, [&] (int start) { onBlock (processor, start); });
    };
    const auto unchanged = withMix ([] (PluginProcessor&, int) {});
    const auto moved = withMix ([] (PluginProcessor& p, int start) {
        if (start == 20 * blockSize)
            floatParameter (p, "mix") = 25.0f;
    });
    const auto fromStart = withMix ([] (PluginProcessor& p, int start) {
        if (start == 0)
            floatParameter (p, "mix") = 25.0f;
    });
    CHECK (largestDifference (moved, unchanged, 0, 20 * blockSize) < 1e-7f);
    CHECK (largestDifference (moved, unchanged, 20 * blockSize, moved.size()) > 0.05f);
    CHECK (largestDifference (moved, fromStart, moved.size() - 512, moved.size()) < 1e-3f);
}

TEST_CASE ("A Mix change does not click", "[plugin][mix][click]")
{
    // SPEC success criterion 3, post-mortem 0012 and 0013: filter wide open, Drive 24 dB Asymmetric, -12 and
    // 0 dBFS, 200 Hz, so dry and wet differ the most. Against the higher of the two settings held steady.
    // The bump is the sine's amplitude changing across the 5 ms window, as for a Drive jump on the symmetric
    // curve (task 0013): up to 0.05 at -12 dBFS, with the 0.29 offset of the DC blocker's measure absent.
    // Click and bump measures printed by: ./Builds/PluginTests "A Mix change does not click" -s | grep "mix"
    for (float peak : { 0.25f, 1.0f })
        for (auto [from, to] : { std::array<float, 2> { 100.0f, 0.0f }, { 0.0f, 100.0f }, { 100.0f, 50.0f } })
        {
            auto output = [&] (float first, float second) {
                PluginProcessor processor;
                floatParameter (processor, "cutoff") = 20000.0f;
                floatParameter (processor, "drive") = 24.0f;
                parameter<juce::AudioParameterChoice> (processor, "driveCurve") = 1;
                floatParameter (processor, "mix") = first;
                return render (processor, 19456, 200.0, peak, [&] (int start) {
                    if (start == 20 * blockSize)
                        floatParameter (processor, "mix") = second;
                });
            };
            const auto moved = output (from, to);
            const std::size_t change = 20 * blockSize;
            const double measure = clickFrom (moved, change);
            const double steady = std::max (clickFrom (output (from, from), change), clickFrom (output (to, to), change));
            const double bump = bumpFrom (moved, change, change + 2400);
            SUCCEED ("mix peak " << peak << ", " << from << " to " << to << ": click " << measure << ", steady " << steady << ", bump " << bump);
            CHECK (measure < 1.2 * steady);
            CHECK (bump < 0.06);
        }
}

TEST_CASE ("A state saved before the Mix loads at 100 %", "[plugin][mix][state]")
{
    // .claude/rules/juce.md: the missing value falls back to the default; others are kept
    juce::XmlElement xml ("MoodzFilter");
    xml.setAttribute ("stateVersion", 1);
    auto* saved = xml.createNewChildElement ("PARAM");
    saved->setAttribute ("id", "cutoff");
    saved->setAttribute ("value", 5000.0);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (xml, block);

    PluginProcessor processor;
    floatParameter (processor, "mix") = 25.0f;
    processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK (floatParameter (processor, "mix").get() == Catch::Approx (100.0f));
    CHECK (floatParameter (processor, "cutoff").get() == Catch::Approx (5000.0f).margin (0.01));
}

TEST_CASE ("The Mix moving with everything else stays finite", "[plugin][mix]")
{
    // Under RealtimeSanitizer in CI. 2 s of noise, resonance 100 %, a bad sample per block, every parameter
    // that moves a path, and the Mix, on every block.
    PluginProcessor processor;
    floatParameter (processor, "resonance") = 100.0f;
    processor.prepareToPlay (48000.0, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    juce::Random random (53);
    const float bad[] { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -3e38f };
    for (int block = 0; block < 188; ++block)
    {
        floatParameter (processor, "mix") = static_cast<float> (block % 5) * 25.0f;
        parameter<juce::AudioParameterChoice> (processor, "filterModel") = block % 9;
        parameter<juce::AudioParameterChoice> (processor, "oversampling") = block % 3;
        floatParameter (processor, "drive") = static_cast<float> (block % 5) * 6.0f;
        floatParameter (processor, "lfoDepth") = -4.0f + static_cast<float> (block % 9);
        floatParameter (processor, "gainOut") = block % 3 == 0 ? -60.0f : 6.0f;
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (channel, i, 0.25f * (random.nextFloat() * 2.0f - 1.0f));
        buffer.setSample (block % 2, block % blockSize, bad[block % 3]);
        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < blockSize; ++i)
                REQUIRE (std::isfinite (buffer.getSample (channel, i)));
    }
}
