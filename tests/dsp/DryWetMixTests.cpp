#include "dsp/DryWetMix.h"
#include "dsp/FilterModels.h"
#include "dsp/OversampledFilterChain.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <vector>

using Catch::Approx;
using moodz::dsp::FilterChoice;
using moodz::dsp::OversampledFilterChain;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int latency = OversampledFilterChain::latencySamples;
    constexpr int rampLength = 960; // 20 ms at 48 kHz, as Gain out
    constexpr int off = 0, twice = 1, fourTimes = 2;

    struct Settings
    {
        int factor = twice;
        float mix = 1.0f;
        float cutoff = 1000.0f;
        float resonance = 0.2f;
        float driveDb = 0.0f;
        int driveCurve = 0;
        float lfoDepth = 0.0f;
        std::size_t variant = 0;
        double hostRate = sampleRate;
    };

    std::unique_ptr<OversampledFilterChain> preparedChain (const Settings& s)
    {
        auto chain = std::make_unique<OversampledFilterChain>();
        chain->setCutoffHz (s.cutoff);
        chain->setResonance (s.resonance);
        chain->setFactorIndex (s.factor);
        chain->select (FilterChoice { s.variant, 0.5f });
        chain->setDriveDb (s.driveDb);
        chain->setDriveCurveIndex (s.driveCurve);
        chain->setLfoDepthOctaves (s.lfoDepth);
        chain->setMix (s.mix);
        chain->prepare (s.hostRate, 512);
        chain->reset();
        return chain;
    }

    void run (OversampledFilterChain& chain, std::span<float> samples, int blockSize = 512)
    {
        const int total = static_cast<int> (samples.size());
        for (int start = 0; start < total; start += blockSize)
        {
            const int length = std::min (blockSize, total - start);
            const std::array<std::span<float>, 1> channels { samples.subspan (static_cast<std::size_t> (start), static_cast<std::size_t> (length)) };
            chain.process (channels, length);
        }
    }

    std::vector<float> sine (double frequency, float peak, int length, double rate = sampleRate)
    {
        std::vector<float> samples (static_cast<std::size_t> (length));
        for (int i = 0; i < length; ++i)
            samples[static_cast<std::size_t> (i)] = peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * i / rate));
        return samples;
    }

    // Deterministic noise (32-bit LCG, Numerical Recipes constants), peak 0.25
    std::vector<float> noise (int length, std::uint32_t seed)
    {
        std::vector<float> samples (static_cast<std::size_t> (length));
        for (auto& s : samples)
        {
            seed = seed * 1664525u + 1013904223u;
            s = 0.25f * (static_cast<float> (seed) / 4294967296.0f * 2.0f - 1.0f);
        }
        return samples;
    }

    bool allFinite (const std::vector<float>& samples)
    {
        return std::all_of (samples.begin(), samples.end(), [] (float s) { return std::isfinite (s); });
    }

    // A NaN on either side counts as a large difference: std::max alone would skip it
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

    // The input delayed by the constant latency: what Mix 0 % must give
    std::vector<float> delayed (const std::vector<float>& input)
    {
        std::vector<float> output (input.size(), 0.0f);
        std::copy (input.begin(), input.end() - latency, output.begin() + latency);
        return output;
    }

    double rms (const std::vector<float>& samples, std::size_t from)
    {
        double sum = 0.0;
        for (std::size_t i = from; i < samples.size(); ++i)
            sum += static_cast<double> (samples[i]) * samples[i];
        return std::sqrt (sum / static_cast<double> (samples.size() - from));
    }
}

TEST_CASE ("Back at Mix 100 % the output is the wet signal alone", "[dsp][mix]")
{
    // Goal of task 0017. Against the chain before the task, the golden references answer (`compare`, every
    // reference unchanged). Here: a Mix that went down and came back leaves nothing of the dry path, with
    // Drive and an LFO on, against a chain that stayed at 100 %.
    const auto input = noise (16384, 131u);
    const Settings settings { .driveDb = 12.0f, .driveCurve = 1, .lfoDepth = 1.0f };
    auto steady = preparedChain (settings);
    auto reference = input;
    run (*steady, reference);
    auto moved = preparedChain (settings);
    auto output = input;
    std::span<float> all { output };
    run (*moved, all.first (2048));
    moved->setMix (0.5f);
    run (*moved, all.subspan (2048, 2048));
    moved->setMix (1.0f);
    run (*moved, all.subspan (4096));
    CHECK (allFinite (output));
    CHECK (largestDifference (output, reference, 4096 + rampLength, output.size()) < 1e-7f);
    CHECK (largestDifference (output, reference, 2048, 4096) > 1e-3f);
}

TEST_CASE ("At Mix 0 % the output is the input delayed by the latency", "[dsp][mix]")
{
    // SPEC, ADR 0009: whatever the Model, Drive, Oversampling or LFO, the same as bypass
    const auto input = noise (8192, 137u);
    const auto expected = delayed (input);
    for (int factor : { off, twice, fourTimes })
        for (std::size_t variant : { std::size_t { 0 }, std::size_t { 6 }, std::size_t { 16 } })
            for (float driveDb : { 0.0f, 24.0f })
            {
                INFO ("factor index " << factor << ", variant " << variant << ", Drive " << driveDb);
                auto chain = preparedChain ({ .factor = factor, .mix = 0.0f, .resonance = 1.0f, .driveDb = driveDb, .driveCurve = 1, .lfoDepth = 3.0f, .variant = variant });
                auto output = input;
                run (*chain, output);
                CHECK (largestDifference (output, expected, 0, output.size()) < 1e-6f);
            }
}

TEST_CASE ("At Mix 0 % a crossfade in the chain does not leak", "[dsp][mix]")
{
    // Code review of task 0017: an Oversampling change and a Model change run while the Mix is at 0 %. The
    // blend comes after the factor crossfade, so the output stays the input delayed.
    const auto input = noise (16384, 163u);
    const auto expected = delayed (input);
    auto chain = preparedChain ({ .mix = 0.0f, .cutoff = 200.0f, .driveDb = 24.0f });
    auto output = input;
    std::span<float> all { output };
    run (*chain, all.first (4096));
    chain->setFactorIndex (fourTimes);
    run (*chain, all.subspan (4096, 4096));
    chain->select (FilterChoice { 6, 0.5f });
    run (*chain, all.subspan (8192));
    CHECK (largestDifference (output, expected, 0, output.size()) < 1e-6f);
}

TEST_CASE ("Dry and wet are aligned at Mix 50 %", "[dsp][mix]")
{
    // SPEC: no comb. An impulse, filter wide open, resonance 0, each factor: one peak, at the latency,
    // within 1 sample, and no second one standing out. Printed by:
    // ./Builds/DspTests "Dry and wet are aligned at Mix 50 %" -s | grep "second"
    for (int factor : { off, twice, fourTimes })
    {
        INFO ("factor index " << factor);
        std::vector<float> output (1024, 0.0f);
        output[100] = 1.0f;
        auto chain = preparedChain ({ .factor = factor, .mix = 0.5f, .cutoff = 20000.0f, .resonance = 0.0f });
        run (*chain, output);
        CHECK (allFinite (output));
        const auto peak = std::max_element (output.begin(), output.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
        CHECK (std::abs (std::distance (output.begin(), peak) - (100 + latency)) <= 1);
        const float top = std::abs (*peak);
        CHECK (top > 0.5f);
        // Nothing beyond 2 samples from the peak reaches 40 % of it, as a dry impulse further away would
        float second = 0.0f;
        for (std::size_t i = 0; i < output.size(); ++i)
            if (std::abs (static_cast<std::ptrdiff_t> (i) - std::distance (output.begin(), peak)) > 2)
                second = std::max (second, std::abs (output[i]));
        SUCCEED ("second peak, factor index " << factor << ": " << second / top);
        CHECK (second / top < 0.4f);
    }
}

TEST_CASE ("Mix 50 % has no comb with the filter wide open", "[dsp][mix]")
{
    // SPEC. Sines at -12 dBFS, cutoff 20 kHz, resonance 0: level at Mix 50 % against the blend of a dry at 0 dB
    // and that wet's level. What is left is the filter's own phase near its cutoff, not a misalignment: the
    // dry path's exact delay is checked at Mix 0 %. Printed by:
    // ./Builds/DspTests "Mix 50 % has no comb with the filter wide open" -s | grep "dip"
    for (int factor : { off, twice, fourTimes })
        for (double frequency : { 1000.0, 5000.0, 10000.0, 15000.0 })
        {
            auto level = [&] (float mix) {
                auto chain = preparedChain ({ .factor = factor, .mix = mix, .cutoff = 20000.0f, .resonance = 0.0f });
                auto output = sine (frequency, 0.25f, 9600);
                run (*chain, output);
                return rms (output, 4800);
            };
            const double dry = 0.25 / std::numbers::sqrt2;
            const double mixed = 20.0 * std::log10 (level (0.5f) / dry), wet = 20.0 * std::log10 (level (1.0f) / dry);
            // What the blend of a dry at 0 dB and that wet should give, if only their levels differ
            const double expected = 20.0 * std::log10 (0.5 + 0.5 * std::pow (10.0, wet / 20.0));
            INFO ("factor index " << factor << ", " << frequency << " Hz, wet " << wet << " dB, mixed " << mixed << " dB");
            SUCCEED ("dip factor index " << factor << ", " << frequency << " Hz: " << mixed - expected << " dB");
            CHECK (std::abs (mixed - expected) < 2.0);
        }
}

TEST_CASE ("The blend is linear", "[dsp][mix]")
{
    // Decision notes. 10 kHz sine at -12 dBFS, Cytomic SVF Low-pass at 20 Hz, resonance 0: the wet part is
    // gone, so the output is the dry weight: Mix 25 % gives 75 % of the input, Mix 75 % gives 25 %.
    const double input = 0.25 / std::numbers::sqrt2;
    for (auto [mix, weight] : { std::array<double, 2> { 0.25, 0.75 }, { 0.75, 0.25 }, { 0.5, 0.5 } })
    {
        INFO ("Mix " << mix);
        auto chain = preparedChain ({ .mix = static_cast<float> (mix), .cutoff = 20.0f, .resonance = 0.0f });
        auto output = sine (10000.0, 0.25f, 9600);
        run (*chain, output);
        CHECK (20.0 * std::log10 (rms (output, 4800) / (input * weight)) == Approx (0.0).margin (0.1));
    }
}

TEST_CASE ("A Mix change ramps over 20 ms", "[dsp][mix]")
{
    // SPEC: Mix smoothed, as Gain out. 100 % to 0 % at 2048: from the change + 959 samples the output is the
    // input delayed, and one sample before it is not.
    constexpr std::size_t change = 2048;
    const auto input = sine (440.0, 0.25f, 8192);
    const auto expected = delayed (input);
    auto chain = preparedChain ({ .mix = 1.0f, .cutoff = 200.0f });
    auto output = input;
    std::span<float> all { output };
    run (*chain, all.first (change));
    chain->setMix (0.0f);
    run (*chain, all.subspan (change));
    CHECK (largestDifference (output, expected, change + rampLength - 1, output.size()) < 1e-6f);
    CHECK (std::abs (output[change + rampLength - 2] - expected[change + rampLength - 2]) > 1e-5f);
    // Half way half way: dry and wet are far apart here at the peak of the difference
    auto steady = preparedChain ({ .mix = 1.0f, .cutoff = 200.0f });
    auto wet = input;
    run (*steady, wet);
    const std::size_t middle = change + rampLength / 2 - 1;
    CHECK (output[middle] == Approx (0.5 * wet[middle] + 0.5 * expected[middle]).margin (1e-4));
}

TEST_CASE ("The Mix ramp keeps its time at another sample rate", "[dsp][mix]")
{
    // Post-mortem 0015. At 44.1 kHz 20 ms is 882 samples: on target from the change + 881, not before
    constexpr std::size_t change = 2048;
    constexpr double rate = 44100.0;
    const auto input = sine (440.0, 0.25f, 8192, rate);
    const auto expected = delayed (input);
    auto chain = preparedChain ({ .mix = 1.0f, .cutoff = 200.0f, .hostRate = rate });
    auto output = input;
    std::span<float> all { output };
    run (*chain, all.first (change));
    chain->setMix (0.0f);
    run (*chain, all.subspan (change));
    CHECK (largestDifference (output, expected, change + 881, output.size()) < 1e-6f);
    CHECK (std::abs (output[change + 880] - expected[change + 880]) > 1e-5f);
}

TEST_CASE ("The Mix does not depend on the block size", "[dsp][mix]")
{
    // .claude/rules/dsp.md: a Mix change at 4096 and a factor change at 8192
    const auto input = noise (16384, 139u);
    auto renderAt = [&] (int blockSize) {
        auto samples = input;
        auto chain = preparedChain ({ .mix = 0.25f, .driveDb = 12.0f });
        std::span<float> all { samples };
        run (*chain, all.first (4096), blockSize);
        chain->setMix (0.75f);
        run (*chain, all.subspan (4096, 4096), blockSize);
        chain->setFactorIndex (fourTimes);
        run (*chain, all.subspan (8192), blockSize);
        return samples;
    };
    const auto reference = renderAt (4096);
    CHECK (allFinite (reference));
    for (int blockSize : { 1, 7, 64, 480, 512, 2048 })
    {
        INFO ("block size " << blockSize);
        CHECK (largestDifference (renderAt (blockSize), reference, 0, reference.size()) < 1e-6f);
    }
}

TEST_CASE ("Bad input samples stay out of the dry path", "[dsp][mix][non-finite]")
{
    // Task 0014. At Mix 0 % and 50 %, a bad sample counts as 0 in the dry path too
    const auto input = noise (4096, 149u);
    for (float mix : { 0.0f, 0.5f })
        for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 3e38f })
        {
            INFO ("Mix " << mix << ", bad sample " << bad);
            auto samples = input;
            samples[1000] = bad;
            auto chain = preparedChain ({ .mix = mix });
            run (*chain, samples);
            CHECK (allFinite (samples));
            auto zeroed = input;
            zeroed[1000] = 0.0f;
            auto reference = preparedChain ({ .mix = mix });
            run (*reference, zeroed);
            CHECK (largestDifference (samples, zeroed, 0, samples.size()) < 1e-6f);
        }
}

TEST_CASE ("Each channel is mixed alike and on its own", "[dsp][mix]")
{
    // SPEC: linked settings, separate signals, with a dry path each
    auto left = sine (1000.0, 0.25f, 4096), right = sine (300.0, 0.125f, 4096);
    auto alone = [] (std::vector<float> samples) {
        auto chain = preparedChain ({ .mix = 0.5f });
        run (*chain, samples);
        return samples;
    };
    const auto expectedLeft = alone (left), expectedRight = alone (right);
    auto chain = preparedChain ({ .mix = 0.5f });
    for (int start = 0; start < 4096; start += 512)
    {
        const auto offset = static_cast<std::size_t> (start);
        const std::array<std::span<float>, 2> channels { std::span<float> { left }.subspan (offset, 512), std::span<float> { right }.subspan (offset, 512) };
        chain->process (channels, 512);
    }
    CHECK (largestDifference (left, expectedLeft, 0, left.size()) < 1e-6f);
    CHECK (largestDifference (right, expectedRight, 0, right.size()) < 1e-6f);
}

TEST_CASE ("A reset clears the dry path and jumps to the Mix", "[dsp][mix]")
{
    // reset() in the header of OversampledFilterChain: no old signal comes out of the dry path, and no ramp
    auto chain = preparedChain ({ .mix = 0.0f });
    auto signal = noise (2048, 157u);
    run (*chain, signal);
    chain->reset();
    std::vector<float> silence (256, 0.0f);
    run (*chain, silence);
    for (auto s : silence)
        REQUIRE (std::abs (s) < 1e-9f);

    // Moved to 100 % just before a reset: all wet at once
    auto moved = preparedChain ({ .mix = 0.0f, .cutoff = 20.0f, .resonance = 0.0f });
    moved->setMix (1.0f);
    moved->reset();
    auto output = sine (10000.0, 0.25f, 512);
    run (*moved, output);
    CHECK (rms (output, 256) < 0.001);
}

TEST_CASE ("All dry ignores a non-finite wet signal", "[dsp][mix]")
{
    // Code review of task 0017: 0 x NaN is NaN, and all dry means all dry, whatever the wet path holds.
    moodz::dsp::DryWetMix mix;
    mix.setAmount (0.0f);
    mix.prepare (sampleRate, 512, 0);
    std::array<float, 4> samples { 0.1f, 0.2f, 0.3f, 0.4f };
    const std::array<std::span<float>, 1> channels { std::span<float> { samples } };
    mix.captureDry (channels, 4);
    samples = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f };
    mix.mix (channels, 4);
    CHECK (samples[0] == Approx (0.1f).margin (1e-7));
    CHECK (samples[1] == Approx (0.2f).margin (1e-7));
    CHECK (samples[2] == Approx (0.3f).margin (1e-7));
    CHECK (samples[3] == Approx (0.4f).margin (1e-7));
}

TEST_CASE ("Mix values out of range are bounded", "[dsp][mix]")
{
    const auto input = noise (4096, 151u);
    auto rendered = [&] (float mix) {
        auto chain = preparedChain ({ .mix = mix });
        auto output = input;
        run (*chain, output);
        return output;
    };
    CHECK (largestDifference (rendered (9.0f), rendered (1.0f), 0, input.size()) < 1e-7f);
    CHECK (largestDifference (rendered (-9.0f), rendered (0.0f), 0, input.size()) < 1e-7f);
    auto chain = preparedChain ({ .mix = 0.0f });
    chain->setMix (std::numeric_limits<float>::quiet_NaN());
    chain->setMix (std::numeric_limits<float>::infinity());
    auto output = input;
    run (*chain, output);
    CHECK (largestDifference (output, rendered (0.0f), 0, input.size()) < 1e-7f);
}
