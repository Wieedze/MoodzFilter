#include "plugin/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include <vector>

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
    double clickMeasure (const std::function<void (PluginProcessor&, int)>& onBlock, int blocks = changeBlock + 6, int window = 2400, float peak = 0.25f)
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
                    buffer.setSample (channel, i, peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * 200.0 * (block * blockSize + i) / 48000.0)));
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
    // With preRoll forced to 0 in OversampledFilterChain.cpp: 0.0016, 0.0017, 0.0012. As it is:
    // 0.00017, the sine alone. The bound stays close to that.
    constexpr double maximumOversamplingClick = 0.00025;
    const auto offTo2x = clickMeasure (change ("oversampling", 0.0f, 1.0f));
    const auto twoTo4x = clickMeasure (change ("oversampling", 1.0f, 2.0f));
    const auto fourToOff = clickMeasure (change ("oversampling", 2.0f, 0.0f));
    SUCCEED ("click Off to 2x " << offTo2x << ", 2x to 4x " << twoTo4x << ", 4x to Off " << fourToOff);
    CHECK (offTo2x < maximumOversamplingClick);
    CHECK (twoTo4x < maximumOversamplingClick);
    CHECK (fourToOff < maximumOversamplingClick);
}

TEST_CASE ("An Oversampling change does not click with the filter wide open", "[plugin][crossfade][oversampling]")
{
    // Code review of task 0012: cutoff 20 kHz let the incoming path's cold start through, 0.0070 at
    // -12 dBFS and 0.028 at 0 dBFS before the pre-roll. Sine at -12 and 0 dBFS. The sine alone gives
    // about 1.7e-4 and 6.9e-4, and so does the change now. Click measures printed by:
    // ./Builds/PluginTests "An Oversampling change does not click with the filter wide open" -s | grep "click"
    auto wide = [] (PluginProcessor& p) { set (p, "cutoff", 20000.0f); };
    for (float peak : { 0.25f, 1.0f })
        for (auto [from, to] : { std::array<float, 2> { 0.0f, 1.0f }, { 1.0f, 2.0f }, { 2.0f, 0.0f } })
        {
            const auto measure = clickMeasure (change ("oversampling", from, to, wide), changeBlock + 6, 2400, peak);
            SUCCEED ("click peak " << peak << ", setting " << from << " to " << to << ": " << measure);
            // Nothing stands out of the signal's own curve: at most 20 % above the sine alone
            CHECK (measure < 1.2 * 6.9e-4 * peak);
        }
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

TEST_CASE ("Drive and Drive curve changes do not click", "[plugin][crossfade][drive]")
{
    // SPEC success criterion 3, post-mortem 0012: filter wide open, -12 and 0 dBFS. Each change is
    // measured against the setting it ends on, held steady, since a saturated sine has a sharper
    // curve than a clean one. Click measures printed by:
    // ./Builds/PluginTests "Drive and Drive curve changes do not click" -s | grep "click"
    auto steady = [] (float drive, float curve) {
        return [=] (PluginProcessor& p, int block) {
            if (block == -1)
            {
                set (p, "cutoff", 20000.0f);
                set (p, "drive", drive);
                set (p, "driveCurve", curve);
            }
        };
    };
    auto wide = [] (float curve) { return [=] (PluginProcessor& p) { set (p, "cutoff", 20000.0f); set (p, "driveCurve", curve); }; };
    auto wideAtFullDrive = [] (PluginProcessor& p) { set (p, "cutoff", 20000.0f); set (p, "drive", 24.0f); };
    for (float peak : { 0.25f, 1.0f })
    {
        for (float curve : { 0.0f, 1.0f })
        {
            const auto reference = clickMeasure (steady (24.0f, curve), changeBlock + 6, 2400, peak);
            const auto rising = clickMeasure (change ("drive", 0.0f, 24.0f, wide (curve)), changeBlock + 6, 2400, peak);
            SUCCEED ("click peak " << peak << ", curve " << curve << ", Drive 0 to 24 dB: " << rising << ", steady " << reference);
            CHECK (rising < 1.2 * reference);
        }
        const auto toAsymmetric = clickMeasure (change ("driveCurve", 0.0f, 1.0f, wideAtFullDrive), changeBlock + 6, 2400, peak);
        const auto asymmetricSteady = clickMeasure (steady (24.0f, 1.0f), changeBlock + 6, 2400, peak);
        const auto symmetricSteady = clickMeasure (steady (24.0f, 0.0f), changeBlock + 6, 2400, peak);
        SUCCEED ("click peak " << peak << ", curve change: " << toAsymmetric << ", steady " << symmetricSteady << " and " << asymmetricSteady);
        CHECK (toAsymmetric < 1.2 * std::max (asymmetricSteady, symmetricSteady));
    }
}

TEST_CASE ("An Oversampling change at full Drive does not click", "[plugin][crossfade][drive]")
{
    // Post-mortem 0012: filter wide open, Drive 24 dB, both curves. Click measures printed by:
    // ./Builds/PluginTests "An Oversampling change at full Drive does not click" -s | grep "click"
    for (float peak : { 0.25f, 1.0f })
        for (float curve : { 0.0f, 1.0f })
        {
            auto driven = [=] (PluginProcessor& p) { set (p, "cutoff", 20000.0f); set (p, "drive", 24.0f); set (p, "driveCurve", curve); };
            for (auto [from, to] : { std::array<float, 2> { 0.0f, 1.0f }, { 1.0f, 2.0f }, { 2.0f, 0.0f } })
            {
                auto held = [=] (float setting) {
                    return [=] (PluginProcessor& p, int block) { if (block == -1) { driven (p); set (p, "oversampling", setting); } };
                };
                const auto measure = clickMeasure (change ("oversampling", from, to, driven), changeBlock + 6, 2400, peak);
                const auto reference = std::max (clickMeasure (held (from), changeBlock + 6, 2400, peak), clickMeasure (held (to), changeBlock + 6, 2400, peak));
                SUCCEED ("click peak " << peak << ", curve " << curve << ", setting " << from << " to " << to << ": " << measure << ", steady " << reference);
                CHECK (measure < 1.2 * reference);
            }
        }
}

namespace
{
    // A low-frequency bump, which the click measure cannot see: the largest |mean| over any 5 ms (one
    // period of the 200 Hz sine, so the sine itself averages to 0), left output, over the 100 ms after
    // the change. Same signal and settings as clickMeasure.
    double bumpMeasure (const std::function<void (PluginProcessor&, int)>& onBlock, float peak)
    {
        PluginProcessor processor;
        set (processor, "cutoff", 20000.0f);
        set (processor, "resonance", 20.0f);
        onBlock (processor, -1);
        processor.prepareToPlay (48000.0, blockSize);

        constexpr int period = 240;
        const int from = changeBlock * blockSize, to = from + 4800;
        std::vector<float> output;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        for (int block = 0; block * blockSize < to + period; ++block)
        {
            onBlock (processor, block);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (channel, i, peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * 200.0 * (block * blockSize + i) / 48000.0)));
            processor.processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
                output.push_back (buffer.getSample (0, i));
        }
        double largest = 0.0;
        for (int start = from; start < to; ++start)
        {
            double sum = 0.0;
            for (int i = 0; i < period; ++i)
                sum += static_cast<double> (output[static_cast<size_t> (start + i)]);
            largest = std::max (largest, std::abs (sum / period));
        }
        return largest;
    }

}

TEST_CASE ("A Drive curve change at full Drive leaves no low-frequency bump", "[plugin][crossfade][drive]")
{
    // Code review of task 0013: 0.07 to 0.09 when the DC blocker stopped on Symmetric (the commit
    // before it). What is left is the waveform changing during the blend, no offset. Printed by:
    // ./Builds/PluginTests "A Drive curve change at full Drive leaves no low-frequency bump" -s | grep "bump"
    auto driven = [] (PluginProcessor& p) { set (p, "drive", 24.0f); };
    for (float peak : { 0.25f, 1.0f })
        for (auto [from, to] : { std::array<float, 2> { 0.0f, 1.0f }, { 1.0f, 0.0f } })
        {
            const auto measure = bumpMeasure (change ("driveCurve", from, to, driven), peak);
            SUCCEED ("bump peak " << peak << ", curve " << from << " to " << to << ": " << measure);
            CHECK (measure < 0.01);
        }
}

TEST_CASE ("An Oversampling change at full Drive leaves no low-frequency bump", "[plugin][crossfade][drive]")
{
    // Code review of task 0013: the incoming path takes over the DC estimate of the outgoing one.
    // Without takeDcEstimateFrom in OversampledFilterChain.cpp: 0.07 to 0.09. Printed by:
    // ./Builds/PluginTests "An Oversampling change at full Drive leaves no low-frequency bump" -s | grep "bump"
    auto driven = [] (PluginProcessor& p) { set (p, "drive", 24.0f); set (p, "driveCurve", 1.0f); };
    for (float peak : { 0.25f, 1.0f })
        for (auto [from, to] : { std::array<float, 2> { 0.0f, 1.0f }, { 1.0f, 2.0f }, { 2.0f, 0.0f } })
        {
            const auto measure = bumpMeasure (change ("oversampling", from, to, driven), peak);
            SUCCEED ("bump peak " << peak << ", setting " << from << " to " << to << ": " << measure);
            CHECK (measure < 0.005);
        }
}

TEST_CASE ("A sudden Drive jump leaves a bounded low-frequency bump", "[plugin][crossfade][drive]")
{
    // The asymmetric offset appears in 20 ms, faster than the 10 Hz DC blocker follows: a jump in one
    // block is the worst case, and slower automation gives less. Symmetric has no offset: there the
    // measure only sees the sine's amplitude rising across the 5 ms window. Printed by:
    // ./Builds/PluginTests "A sudden Drive jump leaves a bounded low-frequency bump" -s | grep "bump"
    auto asymmetric = [] (PluginProcessor& p) { set (p, "driveCurve", 1.0f); };
    for (float peak : { 0.25f, 1.0f })
    {
        const auto measure = bumpMeasure (change ("drive", 0.0f, 24.0f, asymmetric), peak);
        const auto symmetricJump = bumpMeasure (change ("drive", 0.0f, 24.0f), peak);
        SUCCEED ("bump peak " << peak << ", Drive 0 to 24 dB: asymmetric " << measure << ", symmetric " << symmetricJump);
        CHECK (measure < 0.2);
        CHECK (symmetricJump < 0.06);
    }
}
