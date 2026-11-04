#include "plugin/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>

namespace
{
    juce::RangedAudioParameter& parameter (PluginProcessor& processor, const juce::String& id)
    {
        for (auto* p : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->paramID == id)
                return *ranged;
        FAIL ("no parameter with ID " << id);
        throw std::logic_error ("unreachable");
    }

    void set (PluginProcessor& processor, const juce::String& id, float value)
    {
        auto& p = parameter (processor, id);
        p.setValueNotifyingHost (p.convertTo0to1 (value));
    }

    // Model entries (ADR 0007) and Vintage variants (ADR 0008)
    constexpr float cytomicLowPass = 0, vintageLadder = 4, diodeLadder = 6, k35LowPass = 7;
    constexpr float rungeKuttaCompensated = 1, huovilainen = 2;

    constexpr int blockSize = 512;
    constexpr int changeBlock = 20; // the change lands at sample 10240, about 0.21 s in

    // Click measure (task 0011): the largest |y[n] - 2 y[n-1] + y[n-2]| of the left output over
    // the window after the change (default 50 ms, 2400 samples), on a 200 Hz sine at -12 dBFS, cutoff 5 kHz,
    // resonance 20 %. onBlock sets the parameters before each block (block index given).
    double clickMeasure (const std::function<void (PluginProcessor&, int)>& onBlock, int blocks = changeBlock + 6, int window = 2400)
    {
        PluginProcessor processor;
        set (processor, "cutoff", 5000.0f);
        set (processor, "resonance", 20.0f);
        onBlock (processor, -1); // the starting settings
        processor.prepareToPlay (48000.0, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        float y1 = 0.0f, y2 = 0.0f;
        double largest = 0.0;
        const int from = changeBlock * blockSize, to = from + window;
        for (int block = 0; block < blocks; ++block)
        {
            onBlock (processor, block);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (channel, i, 0.25f * static_cast<float> (std::sin (2.0 * std::numbers::pi * 200.0 * (block * blockSize + i) / 48000.0)));
            processor.processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
            {
                const int n = block * blockSize + i;
                const float y = buffer.getSample (0, i);
                if (n >= from && n < to)
                    largest = std::max (largest, static_cast<double> (std::abs (y - 2.0f * y1 + y2)));
                y2 = y1;
                y1 = y;
            }
        }
        return largest;
    }

    // Starting settings at block -1, the change at changeBlock
    std::function<void (PluginProcessor&, int)> change (const juce::String& id, float before, float after, std::function<void (PluginProcessor&)> setup = {})
    {
        return [=] (PluginProcessor& processor, int block) {
            if (block == -1)
            {
                if (setup)
                    setup (processor);
                set (processor, id, before);
            }
            else if (block == changeBlock)
                set (processor, id, after);
        };
    }

    // Measured before the crossfade (task 0011 red run): 0.208 Cytomic SVF to Diode, 0.128 Vintage
    // to K35, 0.091 Diode slope step, 0.106 Vintage variant, 0.151 slope wobble. With it: 0.00036,
    // 0.00025, 0.00015, 0.00016, 0.00026, against about 0.00017 for the sine alone. The bound is
    // more than 90 times under the smallest red value (commands in each test).
    constexpr double maximumClick = 0.001;
}

TEST_CASE ("A Model change does not click", "[plugin][crossfade]")
{
    // Click measures printed by: ./Builds/PluginTests "A Model change does not click" -s | grep "click"
    const auto toDiode = clickMeasure (change ("filterModel", cytomicLowPass, diodeLadder));
    const auto toK35 = clickMeasure (change ("filterModel", vintageLadder, k35LowPass));
    SUCCEED ("click Cytomic SVF Low-pass to Diode Ladder " << toDiode << ", Vintage Ladder to K35 Low-pass " << toK35);
    CHECK (toDiode < maximumClick);
    CHECK (toK35 < maximumClick);
}

TEST_CASE ("A Diode slope step does not click", "[plugin][crossfade]")
{
    // Click measure printed by: ./Builds/PluginTests "A Diode slope step does not click" -s | grep "click"
    const auto measure = clickMeasure (change ("slope", 12.0f, 18.0f, [] (PluginProcessor& p) { set (p, "filterModel", diodeLadder); }));
    SUCCEED ("click Diode slope 12 to 18 dB " << measure);
    CHECK (measure < maximumClick);
}

TEST_CASE ("A Vintage variant change does not click", "[plugin][crossfade]")
{
    // Click measure printed by: ./Builds/PluginTests "A Vintage variant change does not click" -s | grep "click"
    const auto measure = clickMeasure (change ("vintageVariant", rungeKuttaCompensated, huovilainen, [] (PluginProcessor& p) { set (p, "filterModel", vintageLadder); }));
    SUCCEED ("click Vintage Runge-Kutta compensated to Huovilainen " << measure);
    CHECK (measure < maximumClick);
}

TEST_CASE ("An Oversampling change does not click", "[plugin][crossfade][oversampling]")
{
    // SPEC success criterion 3, task 0012. Click measures printed by:
    // ./Builds/PluginTests "An Oversampling change does not click" -s | grep "click"
    // A one sample crossfade gave 0.040, 0.043, 0.031. With 20 ms: 0.0016, 0.0017, 0.0012, above
    // the Model change because the incoming path starts empty (its filters and its oversampler).
    // The bound is more than 15 times under the smallest red value.
    constexpr double maximumOversamplingClick = 0.002;
    const auto offTo2x = clickMeasure (change ("oversampling", 0.0f, 1.0f));
    const auto twoTo4x = clickMeasure (change ("oversampling", 1.0f, 2.0f));
    const auto fourToOff = clickMeasure (change ("oversampling", 2.0f, 0.0f));
    SUCCEED ("click Off to 2x " << offTo2x << ", 2x to 4x " << twoTo4x << ", 4x to Off " << fourToOff);
    CHECK (offTo2x < maximumOversamplingClick);
    CHECK (twoTo4x < maximumOversamplingClick);
    CHECK (fourToOff < maximumOversamplingClick);
}

TEST_CASE ("A Diode slope wobbling across a step does not click", "[plugin][crossfade]")
{
    // Slope alternating 14.9 and 15.1 dB on every block from changeBlock on, for 1 s: every
    // block crosses the 12/18 dB boundary (task 0010 code review). Click measure printed by:
    // ./Builds/PluginTests "A Diode slope wobbling across a step does not click" -s | grep "click"
    const auto measure = clickMeasure (
        [] (PluginProcessor& processor, int block) {
            if (block == -1)
            {
                set (processor, "filterModel", diodeLadder);
                set (processor, "slope", 14.9f);
            }
            else if (block >= changeBlock)
                set (processor, "slope", block % 2 == 0 ? 15.1f : 14.9f);
        },
        changeBlock + 94,
        94 * blockSize);
    SUCCEED ("click Diode slope wobble " << measure);
    CHECK (measure < maximumClick);
}
