#include "dsp/SmoothedGain.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

using moodz::dsp::SmoothedGain;

namespace
{
    constexpr double sampleRate = 48000.0;

    // Expected values computed outside the implementation (Python 3, 10**(dB/20)):
    constexpr float gainPlus6Db = 1.9952623f; // 10**(6/20)
    constexpr float gainPlus24Db = 15.848932f; // 10**(24/20)
    constexpr float gainMinus24Db = 0.0630957f; // 10**(-24/20)
    constexpr int rampSamples = 960; // 48000 * 0.02

    // Processes a mono buffer in one call.
    void processMono (SmoothedGain& gain, std::vector<float>& samples)
    {
        const std::array<std::span<float>, 1> channels { std::span<float> { samples } };
        gain.process (channels, static_cast<int> (samples.size()));
    }

    std::vector<float> sine (float peak, double frequency, int length)
    {
        std::vector<float> samples (static_cast<size_t> (length));
        for (int i = 0; i < length; ++i)
            samples[static_cast<size_t> (i)] = peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * i / sampleRate));
        return samples;
    }

    float peakOf (const std::vector<float>& samples)
    {
        float peak = 0.0f;
        for (auto s : samples)
            peak = std::max (peak, std::abs (s));
        return peak;
    }

    SmoothedGain preparedAt (float gainDb)
    {
        SmoothedGain gain;
        gain.prepare (sampleRate, 512);
        gain.setTargetDb (gainDb);
        gain.reset();
        return gain;
    }
}

TEST_CASE ("SmoothedGain applies the gain in dB", "[dsp][gain]")
{
    // 1 kHz sine at peak 0.25 (-12 dBFS), 48 kHz, 48 samples = one full period sampled every 7.5 degrees
    SECTION ("+6 dB: peak 0.25 * 10^(6/20) = 0.498816")
    {
        auto gain = preparedAt (6.0f);
        auto samples = sine (0.25f, 1000.0, 480);
        processMono (gain, samples);
        CHECK (peakOf (samples) == Catch::Approx (0.25f * gainPlus6Db).margin (1e-5));
    }

    SECTION ("-24 dB: peak 0.25 * 10^(-24/20) = 0.0157739")
    {
        auto gain = preparedAt (-24.0f);
        auto samples = sine (0.25f, 1000.0, 480);
        processMono (gain, samples);
        CHECK (peakOf (samples) == Catch::Approx (0.25f * gainMinus24Db).margin (1e-6));
    }
}

TEST_CASE ("SmoothedGain ramps linearly over 20 ms", "[dsp][gain]")
{
    auto gain = preparedAt (0.0f);
    gain.setTargetDb (6.0f);

    std::vector<float> dc (2000, 1.0f);
    processMono (gain, dc);

    // Largest step of a 960-sample linear ramp from 1 to 10^(6/20): (1.9952623 - 1) / 960
    constexpr float maxStep = (gainPlus6Db - 1.0f) / rampSamples;
    float previous = 1.0f;
    for (auto s : dc)
    {
        CHECK (std::abs (s - previous) <= maxStep + 1e-6f);
        previous = s;
    }

    CHECK (dc[rampSamples / 2] > 1.0f);
    CHECK (dc[rampSamples / 2] < gainPlus6Db);
    for (size_t i = rampSamples - 1; i < dc.size(); ++i)
        CHECK (dc[i] == Catch::Approx (gainPlus6Db).margin (1e-5));
}

TEST_CASE ("SmoothedGain clamps the target to the SPEC range", "[dsp][gain]")
{
    SECTION ("+60 dB is clamped to +24 dB")
    {
        auto gain = preparedAt (60.0f);
        std::vector<float> dc (16, 1.0f);
        processMono (gain, dc);
        CHECK (dc.back() == Catch::Approx (gainPlus24Db).margin (1e-4));
    }

    SECTION ("-60 dB is clamped to -24 dB")
    {
        auto gain = preparedAt (-60.0f);
        std::vector<float> dc (16, 1.0f);
        processMono (gain, dc);
        CHECK (dc.back() == Catch::Approx (gainMinus24Db).margin (1e-6));
    }
}

TEST_CASE ("SmoothedGain ignores a non-finite target", "[dsp][gain]")
{
    auto gain = preparedAt (6.0f);
    gain.setTargetDb (std::numeric_limits<float>::quiet_NaN());
    gain.setTargetDb (std::numeric_limits<float>::infinity());

    std::vector<float> dc (2000, 1.0f);
    processMono (gain, dc);

    for (auto s : dc)
    {
        REQUIRE (std::isfinite (s));
        CHECK (s == Catch::Approx (gainPlus6Db).margin (1e-6));
    }
}

TEST_CASE ("SmoothedGain does not depend on the block size", "[dsp][gain]")
{
    // Deterministic noise (32-bit LCG, Numerical Recipes constants), gain change half way
    std::vector<float> input (4096);
    std::uint32_t state = 12345u;
    for (auto& s : input)
    {
        state = state * 1664525u + 1013904223u;
        s = static_cast<float> (state) / 4294967296.0f * 2.0f - 1.0f;
    }

    auto render = [&] (int blockSize) {
        auto gain = preparedAt (-6.0f);
        auto output = input;
        for (int start = 0; start < static_cast<int> (output.size()); start += blockSize)
        {
            if (start <= 2048 && 2048 < start + blockSize)
                gain.setTargetDb (12.0f);
            const int length = std::min (blockSize, static_cast<int> (output.size()) - start);
            const std::array<std::span<float>, 1> channels { std::span<float> { output }.subspan (static_cast<size_t> (start)) };
            gain.process (channels, length);
        }
        return output;
    };

    // The target changes at the start of the block holding sample 2048, so blocks must
    // align on 2048 for the comparison: 1, 64 and 512 do; 7 and 480 are checked separately.
    const auto reference = render (1);
    for (int blockSize : { 64, 512 })
    {
        const auto output = render (blockSize);
        for (size_t i = 0; i < output.size(); ++i)
            REQUIRE (output[i] == Catch::Approx (reference[i]).margin (1e-7));
    }

    // Odd block sizes: a constant target (no change) must still give identical outputs.
    auto renderConstant = [&] (int blockSize) {
        auto gain = preparedAt (0.0f);
        gain.setTargetDb (9.0f); // ramp in progress across block boundaries
        auto output = input;
        for (int start = 0; start < static_cast<int> (output.size()); start += blockSize)
        {
            const int length = std::min (blockSize, static_cast<int> (output.size()) - start);
            const std::array<std::span<float>, 1> channels { std::span<float> { output }.subspan (static_cast<size_t> (start)) };
            gain.process (channels, length);
        }
        return output;
    };
    const auto constantReference = renderConstant (4096);
    for (int blockSize : { 1, 7, 480 })
    {
        const auto output = renderConstant (blockSize);
        for (size_t i = 0; i < output.size(); ++i)
            REQUIRE (output[i] == Catch::Approx (constantReference[i]).margin (1e-7));
    }
}

TEST_CASE ("SmoothedGain handles a block of 0 samples", "[dsp][gain]")
{
    auto gain = preparedAt (0.0f);
    gain.setTargetDb (6.0f);

    std::vector<float> empty;
    processMono (gain, empty);

    // The ramp has not advanced: the first sample after the empty block is one step above 1
    std::vector<float> dc (1, 1.0f);
    processMono (gain, dc);
    CHECK (dc[0] == Catch::Approx (1.0f + (gainPlus6Db - 1.0f) / rampSamples).margin (1e-6));
}

TEST_CASE ("SmoothedGain processes linked channels identically", "[dsp][gain]")
{
    auto gain = preparedAt (0.0f);
    gain.setTargetDb (-12.0f);
    std::vector<float> left (1000, 0.5f);
    std::vector<float> right (1000, 0.5f);
    const std::array<std::span<float>, 2> channels { std::span<float> { left }, std::span<float> { right } };
    gain.process (channels, 1000);
    for (size_t i = 0; i < left.size(); ++i)
        REQUIRE (left[i] == Catch::Approx (right[i]).margin (1e-9));
}
