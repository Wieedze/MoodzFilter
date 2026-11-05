#include "dsp/FilterModels.h"
#include "dsp/OversampledFilterChain.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>

using moodz::dsp::FilterChoice;
using moodz::dsp::OversampledFilterChain;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int crossfadeLength = 960; // 20 ms at 48 kHz, as task 0011
    constexpr int off = 0, twice = 1, fourTimes = 2;
    constexpr FilterChoice cytomicLowPass { 0, 0.0f }, cytomicHighPass { 1, 0.0f };

    // Deterministic noise (32-bit LCG, Numerical Recipes constants), peak 0.25
    std::vector<float> noise (int length, std::uint32_t seed)
    {
        std::vector<float> samples (static_cast<size_t> (length));
        for (auto& s : samples)
        {
            seed = seed * 1664525u + 1013904223u;
            s = 0.25f * (static_cast<float> (seed) / 4294967296.0f * 2.0f - 1.0f);
        }
        return samples;
    }

    std::unique_ptr<OversampledFilterChain> preparedChain (int factorIndex, FilterChoice choice, float cutoff, float resonance)
    {
        // On the heap: three banks of filters
        auto chain = std::make_unique<OversampledFilterChain>();
        chain->setCutoffHz (cutoff);
        chain->setResonance (resonance);
        chain->setFactorIndex (factorIndex);
        chain->select (choice);
        chain->prepare (sampleRate, 512);
        chain->reset();
        return chain;
    }

    struct Change
    {
        int at;
        int factorIndex; // -1: unchanged
        FilterChoice choice;
    };

    std::vector<float> render (int factorIndex, FilterChoice first, const std::vector<Change>& changes, std::vector<float> samples, int blockSize)
    {
        auto chain = preparedChain (factorIndex, first, 1000.0f, 0.5f);
        const int total = static_cast<int> (samples.size());
        int start = 0;
        size_t next = 0;
        while (start < total)
        {
            if (next < changes.size() && start == changes[next].at)
            {
                if (changes[next].factorIndex >= 0)
                    chain->setFactorIndex (changes[next].factorIndex);
                chain->select (changes[next].choice);
                ++next;
            }
            int length = std::min (blockSize, total - start);
            if (next < changes.size() && changes[next].at < start + length)
                length = changes[next].at - start;
            const std::array<std::span<float>, 1> channels { std::span<float> { samples }.subspan (static_cast<size_t> (start), static_cast<size_t> (length)) };
            chain->process (channels, length);
            start += length;
        }
        return samples;
    }

    float largestDifference (const std::vector<float>& a, const std::vector<float>& b, size_t from, size_t to)
    {
        float largest = 0.0f;
        for (size_t i = from; i < to; ++i)
            largest = std::max (largest, std::abs (a[i] - b[i]));
        return largest;
    }

    double rms (const std::vector<float>& samples, size_t from)
    {
        double sum = 0.0;
        for (size_t i = from; i < samples.size(); ++i)
            sum += static_cast<double> (samples[i]) * samples[i];
        return std::sqrt (sum / static_cast<double> (samples.size() - from));
    }
}

TEST_CASE ("Every factor comes out at the constant latency", "[dsp][oversampling]")
{
    // ADR 0009. Impulse, Cytomic SVF Low-pass wide open (20 kHz, resonance 0): the peak lands at
    // the constant latency in the 3 settings, ± 1 sample for the filter's own short delay.
    for (int index : { off, twice, fourTimes })
    {
        auto chain = preparedChain (index, cytomicLowPass, 20000.0f, 0.0f);
        std::vector<float> samples (2048, 0.0f);
        samples[256] = 1.0f;
        const std::array<std::span<float>, 1> channels { std::span<float> { samples } };
        chain->process (channels, 2048);
        const auto peak = std::max_element (samples.begin(), samples.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) - samples.begin();
        INFO ("factor index " << index << ", peak at " << peak);
        CHECK (std::abs (peak - (256 + OversampledFilterChain::latencySamples)) <= 1);
    }
}

TEST_CASE ("The cutoff does not move with the factor", "[dsp][oversampling]")
{
    // SPEC signal chain: Cytomic SVF Low-pass, cutoff 1 kHz, resonance 20 %, sines at 500 Hz,
    // 1 kHz, 4 kHz at -12 dBFS. Off is the reference; 2x and 4x within ± 0.5 dB
    // (largest 0.35 dB at 4 kHz, 4x: ./Builds/DspTests "The cutoff does not move with the factor" -s).
    for (double hz : { 500.0, 1000.0, 4000.0 })
    {
        std::vector<float> input (9600);
        for (size_t i = 0; i < input.size(); ++i)
            input[i] = 0.25f * static_cast<float> (std::sin (2.0 * std::numbers::pi * hz * static_cast<double> (i) / sampleRate));
        double reference = 0.0;
        for (int index : { off, twice, fourTimes })
        {
            auto chain = preparedChain (index, cytomicLowPass, 1000.0f, 0.2f);
            auto samples = input;
            const std::array<std::span<float>, 1> channels { std::span<float> { samples } };
            chain->process (channels, 9600);
            const double level = rms (samples, 4800);
            if (index == off)
                reference = level;
            const double db = 20.0 * std::log10 (level / reference);
            INFO (hz << " Hz, factor index " << index << ": " << db << " dB against Off");
            CHECK (std::abs (db) < 0.5);
        }
    }
}

TEST_CASE ("Every filter variant stays finite at 4x", "[dsp][oversampling]")
{
    const auto input = noise (9600, 67u);
    for (std::size_t variant = 0; variant < moodz::dsp::filterVariantCount; ++variant)
        for (float extra : { 0.0f, 1.0f })
        {
            auto chain = preparedChain (fourTimes, FilterChoice { variant, extra }, 1000.0f, 1.0f);
            auto samples = input;
            const std::array<std::span<float>, 1> channels { std::span<float> { samples } };
            chain->process (channels, 9600);
            INFO ("variant " << variant << ", extra " << extra);
            for (auto s : samples)
                REQUIRE (std::isfinite (s));
        }
}

TEST_CASE ("A factor change crossfades to the new factor in 20 ms", "[dsp][oversampling]")
{
    // Before the change: the first factor alone. From change + 960: the new factor alone, as if
    // started clean at the change, its input zero before. One sample before, still some of the
    // old: the change also moves to Cytomic HP so the two paths clearly differ there.
    constexpr int change = 4096;
    const auto input = noise (8192, 71u);
    for (auto [from, to] : { std::array<int, 2> { off, twice }, { twice, fourTimes }, { fourTimes, off } })
    {
        INFO ("factor index " << from << " to " << to);
        const auto output = render (from, cytomicLowPass, { { change, to, cytomicHighPass } }, input, 512);
        const auto outgoing = render (from, cytomicLowPass, {}, input, 512);
        auto fromChange = input;
        std::fill_n (fromChange.begin(), change, 0.0f);
        const auto incoming = render (to, cytomicHighPass, {}, fromChange, 512);
        CHECK (largestDifference (output, outgoing, 0, change) < 1e-6f);
        CHECK (largestDifference (output, incoming, change + crossfadeLength, output.size()) < 1e-6f);
        CHECK (std::abs (output[change + crossfadeLength - 1] - incoming[change + crossfadeLength - 1]) > 1e-6f);
    }
}

TEST_CASE ("A factor change during a crossfade waits for it to end", "[dsp][oversampling]")
{
    // Off to 2x at 4096, to 4x at 4608: the second crossfade starts at 4096 + 960 whatever the
    // block size, and from there + 960 the output is 4x alone, started clean at 5056.
    constexpr int secondStart = 4096 + crossfadeLength;
    const auto input = noise (16384, 73u);
    auto fromSecond = input;
    std::fill_n (fromSecond.begin(), secondStart, 0.0f);
    const auto last = render (fourTimes, cytomicLowPass, {}, fromSecond, 512);
    for (int blockSize : { 1, 64, 512, 16384 })
    {
        INFO ("block size " << blockSize);
        const auto output = render (off, cytomicLowPass, { { 4096, twice, cytomicLowPass }, { 4608, fourTimes, cytomicLowPass } }, input, blockSize);
        for (auto s : output)
            REQUIRE (std::isfinite (s));
        CHECK (largestDifference (output, last, secondStart + crossfadeLength, output.size()) < 1e-6f);
    }
}

TEST_CASE ("The oversampled chain does not depend on the block size", "[dsp][oversampling]")
{
    // .claude/rules/dsp.md: a factor change and a Model change at fixed samples
    const auto input = noise (8192, 79u);
    const std::vector<Change> changes { { 2048, twice, cytomicLowPass }, { 4096, -1, cytomicHighPass }, { 6144, fourTimes, cytomicHighPass } };
    const auto reference = render (off, cytomicLowPass, changes, input, 8192);
    for (int blockSize : { 1, 7, 64, 480, 512, 2048 })
    {
        INFO ("block size " << blockSize);
        const auto output = render (off, cytomicLowPass, changes, input, blockSize);
        CHECK (largestDifference (output, reference, 0, output.size()) < 1e-6f);
    }
}

TEST_CASE ("The oversampled chain handles a block of 0 samples and stereo", "[dsp][oversampling]")
{
    auto chain = preparedChain (twice, cytomicLowPass, 1000.0f, 0.5f);
    auto left = noise (4096, 83u);
    auto right = left;
    std::vector<float> empty;
    const std::array<std::span<float>, 1> none { std::span<float> { empty } };
    chain->process (none, 0);
    const std::array<std::span<float>, 2> channels { std::span<float> { left }, std::span<float> { right } };
    chain->process (channels, 4096);
    // Linked stereo (SPEC): the same input gives the same output on both channels
    CHECK (largestDifference (left, right, 0, left.size()) < 1e-6f);
    float largest = 0.0f;
    for (auto s : left)
        largest = std::max (largest, std::abs (s));
    CHECK (largest > 1e-3f);
}

namespace
{
    constexpr int symmetricCurve = 0, asymmetricCurve = 1;

    std::vector<float> sineAt (double frequency, float peak, int length)
    {
        std::vector<float> samples (static_cast<size_t> (length));
        for (int i = 0; i < length; ++i)
            samples[static_cast<size_t> (i)] = peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * i / sampleRate));
        return samples;
    }

    // Amplitude at one frequency, over a whole number of its periods (one point of a DFT)
    double amplitudeAt (const std::vector<float>& samples, size_t from, double frequency)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = from; i < samples.size(); ++i)
        {
            const double phase = 2.0 * std::numbers::pi * frequency * static_cast<double> (i) / sampleRate;
            re += static_cast<double> (samples[i]) * std::cos (phase);
            im += static_cast<double> (samples[i]) * std::sin (phase);
        }
        return 2.0 * std::hypot (re, im) / static_cast<double> (samples.size() - from);
    }

    // The chain wide open, resonance 0, at a steady Drive
    std::vector<float> renderDriven (int factorIndex, float driveDb, int curve, std::vector<float> samples)
    {
        auto chain = preparedChain (factorIndex, cytomicLowPass, 20000.0f, 0.0f);
        chain->setDriveDb (driveDb);
        chain->setDriveCurveIndex (curve);
        chain->reset();
        int start = 0;
        const int total = static_cast<int> (samples.size());
        while (start < total)
        {
            const int length = std::min (512, total - start);
            const std::array<std::span<float>, 1> channels { std::span<float> { samples }.subspan (static_cast<size_t> (start), static_cast<size_t> (length)) };
            chain->process (channels, length);
            start += length;
        }
        return samples;
    }
}

TEST_CASE ("Every factor drives the signal", "[dsp][oversampling][drive]")
{
    // 1 kHz sine at -24 dBFS, cutoff 20 kHz, Drive 12 dB: mostly the drive gain, 12 dB minus what
    // tanh takes at this level, at every factor and for both curves
    const auto input = sineAt (1000.0, 0.0625f, 19200);
    for (int factor : { off, twice, fourTimes })
        for (int curve : { symmetricCurve, asymmetricCurve })
        {
            const auto clean = renderDriven (factor, 0.0f, curve, input);
            const auto driven = renderDriven (factor, 12.0f, curve, input);
            const double db = 20.0 * std::log10 (rms (driven, 9600) / rms (clean, 9600));
            INFO ("factor index " << factor << ", curve " << curve << ": " << db << " dB");
            CHECK (db > 10.0);
            CHECK (db < 12.0);
        }
}

TEST_CASE ("Oversampling lowers the aliases of the drive", "[dsp][oversampling][drive]")
{
    // .claude/rules/dsp.md. 5 kHz sine at -6 dBFS, Drive 24 dB, cutoff 20 kHz, last 1 s of 2 s. The
    // harmonics fall on multiples of 5 kHz; what falls back from above 24 kHz lands on the other
    // multiples of 1 kHz (11 x 5 kHz = 55 kHz, heard at 7 kHz). Largest of those up to 20 kHz against
    // the fundamental: above, 23 kHz holds the 25 kHz harmonic folded in the last stage's transition
    // band, at every factor, out of the pass band (0.43 x 48 kHz). Printed by: ./Builds/DspTests "Oversampling lowers the aliases of the drive" -s | grep "alias"
    const auto input = sineAt (5000.0, 0.5f, 96000);
    auto largestAliasDb = [&] (int factor, int curve) {
        const auto output = renderDriven (factor, 24.0f, curve, input);
        double largest = 0.0;
        for (int k = 1; k <= 20; ++k)
            if (k % 5 != 0)
                largest = std::max (largest, amplitudeAt (output, 48000, k * 1000.0));
        return 20.0 * std::log10 (largest / amplitudeAt (output, 48000, 5000.0));
    };
    for (int curve : { symmetricCurve, asymmetricCurve })
    {
        const auto atOff = largestAliasDb (off, curve), at2x = largestAliasDb (twice, curve), at4x = largestAliasDb (fourTimes, curve);
        SUCCEED ("alias curve " << curve << ": Off " << atOff << " dB, 2x " << at2x << " dB, 4x " << at4x << " dB");
        CHECK (at2x < atOff - 6.0);
        CHECK (at4x < at2x - 6.0);
        CHECK (at4x < -70.0);
    }
}

TEST_CASE ("The oversampled chain with drive does not depend on the block size", "[dsp][oversampling][drive]")
{
    // .claude/rules/dsp.md: Drive up at 2048, the curve at 4096, the factor at 6144, Drive down to
    // 0 dB at 8192 and up again at 12288, past the rest state
    const auto input = noise (16384, 83u);
    auto renderAt = [&] (int blockSize) {
        auto samples = input;
        auto chain = preparedChain (off, cytomicLowPass, 5000.0f, 0.5f);
        const int total = static_cast<int> (samples.size());
        int start = 0;
        auto apply = [&] (int at) {
            if (at == 2048) chain->setDriveDb (24.0f);
            if (at == 4096) chain->setDriveCurveIndex (asymmetricCurve);
            if (at == 6144) chain->setFactorIndex (fourTimes);
            if (at == 8192) chain->setDriveDb (0.0f);
            if (at == 12288) chain->setDriveDb (12.0f);
        };
        while (start < total)
        {
            if (start % 2048 == 0)
                apply (start);
            const int length = std::min ({ blockSize, total - start, 2048 - start % 2048 });
            const std::array<std::span<float>, 1> channels { std::span<float> { samples }.subspan (static_cast<size_t> (start), static_cast<size_t> (length)) };
            chain->process (channels, length);
            start += length;
        }
        return samples;
    };
    const auto reference = renderAt (2048);
    for (int blockSize : { 1, 7, 64, 480, 512 })
    {
        INFO ("block size " << blockSize);
        const auto output = renderAt (blockSize);
        CHECK (largestDifference (output, reference, 0, output.size()) < 1e-6f);
    }
    CHECK (rms (reference, 2048) > 0.0);
    // Drive really acts in this signal: the same render without Drive differs
    const auto undriven = render (off, cytomicLowPass, { { 6144, fourTimes, cytomicLowPass } }, input, 2048);
    CHECK (largestDifference (undriven, reference, 4096, 6144) > 0.01f);
}
