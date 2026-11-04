#include "dsp/Oversampler.h"
#include "dsp/WholeSampleDelay.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

using moodz::dsp::Oversampler;
using moodz::dsp::WholeSampleDelay;

namespace
{
    std::vector<float> sine (double hz, double rate, int length, float peak)
    {
        std::vector<float> samples (static_cast<size_t> (length));
        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * hz * static_cast<double> (i) / rate));
        return samples;
    }

    double rms (const std::vector<float>& samples, size_t from)
    {
        double sum = 0.0;
        for (size_t i = from; i < samples.size(); ++i)
            sum += static_cast<double> (samples[i]) * samples[i];
        return std::sqrt (sum / static_cast<double> (samples.size() - from));
    }

    // Round trip through the oversampler in chunks of chunk samples, nothing done at the high rate
    std::vector<float> roundTrip (int factorIndex, std::vector<float> samples, int chunk)
    {
        Oversampler oversampler;
        oversampler.prepare (factorIndex);
        std::span<float> all { samples };
        for (size_t start = 0; start < samples.size(); start += static_cast<size_t> (chunk))
        {
            const auto part = all.subspan (start, std::min (static_cast<size_t> (chunk), samples.size() - start));
            oversampler.up (0, part);
            oversampler.down (0, part);
        }
        return samples;
    }
}

TEST_CASE ("The oversampler reports its factor and a growing latency", "[dsp][oversampling]")
{
    // ADR 0009: Off has no latency, 4x is the largest and is the plugin's constant latency
    const std::array<int, 3> factors { 1, 2, 4 };
    int previous = -1;
    for (int index : { 0, 1, 2 })
    {
        Oversampler oversampler;
        oversampler.prepare (index);
        CHECK (oversampler.factor() == factors[static_cast<size_t> (index)]);
        CHECK (oversampler.latencySamples() > previous);
        previous = oversampler.latencySamples();
    }
    CHECK (previous == Oversampler::maxLatencySamples);
}

TEST_CASE ("An impulse comes back at the reported latency", "[dsp][oversampling]")
{
    // Linear phase with a whole number of samples of delay (ADR 0009): the peak is exactly there
    for (int index : { 0, 1, 2 })
    {
        Oversampler oversampler;
        oversampler.prepare (index);
        std::vector<float> impulse (1024, 0.0f);
        impulse[100] = 1.0f;
        const auto output = roundTrip (index, impulse, 64);
        const auto peak = std::max_element (output.begin(), output.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) - output.begin();
        INFO ("factor index " << index);
        CHECK (peak == 100 + oversampler.latencySamples());
    }
}

TEST_CASE ("The oversampler round trip is flat in the pass band", "[dsp][oversampling]")
{
    // Sines at -12 dBFS, 48 kHz, 1, 10, 18 kHz, below the pass band edge of Oversampler2xFIR
    // (0.43 x the rate = 20.6 kHz). Expected gain 0 dB, by design of the filter. ± 0.1 dB.
    for (int index : { 1, 2 })
        for (double hz : { 1000.0, 10000.0, 18000.0 })
        {
            const auto input = sine (hz, 48000.0, 8192, 0.25f);
            const auto output = roundTrip (index, input, 256);
            const double db = 20.0 * std::log10 (rms (output, 512) / rms (input, 512));
            INFO ("factor index " << index << ", " << hz << " Hz: " << db << " dB");
            CHECK (std::abs (db) < 0.1);
        }
}

TEST_CASE ("The down stage rejects what would fold back", "[dsp][oversampling]")
{
    // A 36 kHz tone at the 96 kHz high rate would alias to 12 kHz at 48 kHz. Bound from the run,
    // -84.3 dB: ./Builds/DspTests "The down stage rejects what would fold back" -s
    Oversampler oversampler;
    oversampler.prepare (1);
    const auto tone = sine (36000.0, 96000.0, 16384, 0.25f);
    std::vector<float> silence (256, 0.0f), output;
    std::vector<float> block (256);
    for (size_t start = 0; start + 512 <= tone.size(); start += 512)
    {
        auto high = oversampler.up (0, silence);
        std::copy_n (tone.begin() + static_cast<std::ptrdiff_t> (start), 512, high.begin());
        oversampler.down (0, block);
        output.insert (output.end(), block.begin(), block.end());
    }
    const double db = 20.0 * std::log10 (rms (output, 256) / 0.25 * std::numbers::sqrt2);
    INFO ("folded tone: " << db << " dB");
    CHECK (db < -80.0);
}

TEST_CASE ("The oversampler does not depend on the chunk size", "[dsp][oversampling]")
{
    const auto input = sine (3000.0, 48000.0, 4096, 0.25f);
    for (int index : { 1, 2 })
    {
        const auto reference = roundTrip (index, input, 256);
        for (int chunk : { 1, 7, 64, 200 })
        {
            const auto output = roundTrip (index, input, chunk);
            float largest = 0.0f;
            for (size_t i = 0; i < output.size(); ++i)
                largest = std::max (largest, std::abs (output[i] - reference[i]));
            INFO ("factor index " << index << ", chunk " << chunk);
            CHECK (largest < 1e-6f);
        }
    }
}

TEST_CASE ("The whole sample delay delays by exactly its length", "[dsp][oversampling]")
{
    for (int length : { 0, 1, 8, 40, WholeSampleDelay::maxDelay })
    {
        WholeSampleDelay delay;
        delay.prepare (length);
        std::vector<float> samples (256);
        for (size_t i = 0; i < samples.size(); ++i)
            samples[i] = static_cast<float> (i + 1);
        const auto input = samples;
        std::span<float> all { samples };
        // Across two calls: the delay keeps its state between blocks
        const std::array<std::span<float>, 1> first { all.first (30) }, second { all.subspan (30) };
        delay.process (first, 30);
        delay.process (second, 226);
        INFO ("length " << length);
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const auto expected = i < static_cast<size_t> (length) ? 0.0f : input[i - static_cast<size_t> (length)];
            REQUIRE (std::abs (samples[i] - expected) < 1e-6f);
        }
    }
}
