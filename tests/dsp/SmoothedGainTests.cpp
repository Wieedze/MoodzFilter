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
    constexpr float gainMinus59_9Db = 0.0010115794f; // 10**(-59.9/20)
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
    // ADR 0004: Gain out tops at +6 dB
    auto gain = preparedAt (60.0f);
    std::vector<float> dc (16, 1.0f);
    processMono (gain, dc);
    CHECK (dc.back() == Catch::Approx (gainPlus6Db).margin (1e-5));
}

TEST_CASE ("SmoothedGain reaches silence at the floor", "[dsp][gain]")
{
    // ADR 0004: at the silence floor (-60 dB) and below, the gain is exactly 0
    for (float floorOrBelow : { SmoothedGain::silenceFloorDb, -100.0f })
    {
        auto gain = preparedAt (floorOrBelow);
        std::vector<float> dc (64, 1.0f);
        processMono (gain, dc);
        for (auto s : dc)
            REQUIRE (s == 0.0f);
    }

    SECTION ("just above the floor is not silence: 10^(-59.9/20) = 0.00101158")
    {
        auto gain = preparedAt (-59.9f);
        std::vector<float> dc (16, 1.0f);
        processMono (gain, dc);
        CHECK (dc.back() == Catch::Approx (gainMinus59_9Db).margin (1e-7));
    }

    SECTION ("fading to the floor ramps over 960 samples and ends exactly at 0")
    {
        auto gain = preparedAt (0.0f);
        gain.setTargetDb (SmoothedGain::silenceFloorDb);
        std::vector<float> dc (2000, 1.0f);
        processMono (gain, dc);

        constexpr float maxStep = 1.0f / rampSamples;
        float previous = 1.0f;
        for (auto s : dc)
        {
            REQUIRE (std::abs (s - previous) <= maxStep + 1e-6f);
            previous = s;
        }
        CHECK (dc[rampSamples / 2] > 0.0f);
        for (size_t i = rampSamples - 1; i < dc.size(); ++i)
            REQUIRE (dc[i] == 0.0f);
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
    // Deterministic noise (32-bit LCG, Numerical Recipes constants); the target goes from
    // -6 dB to +6 dB exactly at sample 2048, whatever the block size.
    std::vector<float> input (4096);
    std::uint32_t state = 12345u;
    for (auto& s : input)
    {
        state = state * 1664525u + 1013904223u;
        s = static_cast<float> (state) / 4294967296.0f * 2.0f - 1.0f;
    }

    constexpr int changeAt = 2048;
    const int total = static_cast<int> (input.size());

    auto render = [&] (int blockSize) {
        auto gain = preparedAt (-6.0f);
        auto output = input;
        int start = 0;
        while (start < total)
        {
            // Split the block that holds the change so the new target applies from sample 2048 on
            int length = std::min (blockSize, total - start);
            if (start < changeAt && changeAt < start + length)
                length = changeAt - start;
            if (start == changeAt)
                gain.setTargetDb (6.0f);

            const std::array<std::span<float>, 1> channels { std::span<float> { output }.subspan (static_cast<size_t> (start)) };
            gain.process (channels, length);
            start += length;
        }
        return output;
    };

    const auto reference = render (total);
    for (int blockSize : { 1, 7, 64, 480, 512 })
    {
        INFO ("block size " << blockSize);
        const auto output = render (blockSize);
        for (size_t i = 0; i < output.size(); ++i)
            REQUIRE (output[i] == Catch::Approx (reference[i]).margin (1e-7));
    }

    // The change really happened: before it -6 dB, well after it +6 dB (10**(-6/20), 10**(6/20))
    CHECK (reference[100] == Catch::Approx (input[100] * 0.5011872f).margin (1e-6));
    CHECK (reference[4000] == Catch::Approx (input[4000] * gainPlus6Db).margin (1e-5));
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

TEST_CASE ("SmoothedGain survives an invalid sample rate", "[dsp][gain]")
{
    // prepare() with a NaN, zero or absurd rate must not leave a garbage ramp length:
    // the gain still reaches its target within a bounded number of samples and stays finite.
    for (double badRate : { std::numeric_limits<double>::quiet_NaN(), 0.0, -48000.0, 1e300 })
    {
        SmoothedGain gain;
        gain.prepare (badRate, 512);
        gain.setTargetDb (0.0f);
        gain.reset();
        gain.setTargetDb (6.0f);

        std::vector<float> dc (48000, 1.0f);
        processMono (gain, dc);
        for (auto s : dc)
            REQUIRE (std::isfinite (s));
        CHECK (dc.back() == Catch::Approx (gainPlus6Db).margin (1e-5));
    }
}
