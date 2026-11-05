#include "dsp/FilterModels.h"
#include "dsp/OversampledFilterChain.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
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
    auto renderAt = [&] (int blockSize, float peakDriveDb) {
        auto samples = input;
        auto chain = preparedChain (off, cytomicLowPass, 5000.0f, 0.5f);
        const int total = static_cast<int> (samples.size());
        int start = 0;
        auto apply = [&] (int at) {
            if (at == 2048)
                chain->setDriveDb (peakDriveDb);
            if (at == 4096)
                chain->setDriveCurveIndex (asymmetricCurve);
            if (at == 6144)
                chain->setFactorIndex (fourTimes);
            if (at == 8192)
                chain->setDriveDb (0.0f);
            if (at == 12288)
                chain->setDriveDb (peakDriveDb / 2.0f);
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
    const auto reference = renderAt (2048, 24.0f);
    for (int blockSize : { 1, 7, 64, 480, 512 })
    {
        INFO ("block size " << blockSize);
        const auto output = renderAt (blockSize, 24.0f);
        CHECK (largestDifference (output, reference, 0, output.size()) < 1e-6f);
    }
    // Drive really acts in this signal: the same render with Drive at 0 dB differs
    const auto undriven = renderAt (2048, 0.0f);
    CHECK (largestDifference (undriven, reference, 4096, 6144) > 0.01f);
}

TEST_CASE ("The drive comes before the filter", "[dsp][oversampling][drive]")
{
    // SPEC signal chain, task 0013 verifier: every other chain test runs wide open, where the order
    // does not show. 100 Hz sine at -6 dBFS, Drive 24 dB, Cytomic SVF Low-pass at 300 Hz, resonance 0:
    // the filter takes the harmonics of the drive, so the 9th (900 Hz) stays far under what tanh makes
    // on its own, -23 dB against the fundamental:
    // python3 -c "from math import *; h=lambda n:abs(sum(tanh(10**1.2*.5*sin(2*pi*i/4000))*sin(2*pi*n*i/4000) for i in range(4000))); print(20*log10(h(9)/h(1)))"
    // Printed by: ./Builds/DspTests "The drive comes before the filter" -s | grep "ninth"
    auto samples = sineAt (100.0, 0.5f, 96000);
    auto chain = preparedChain (twice, cytomicLowPass, 300.0f, 0.0f);
    chain->setDriveDb (24.0f);
    chain->reset();
    for (int start = 0; start < 96000; start += 512)
    {
        const int length = std::min (512, 96000 - start);
        const std::array<std::span<float>, 1> channels { std::span<float> { samples }.subspan (static_cast<size_t> (start), static_cast<size_t> (length)) };
        chain->process (channels, length);
    }
    const double db = 20.0 * std::log10 (amplitudeAt (samples, 48000, 900.0) / amplitudeAt (samples, 48000, 100.0));
    SUCCEED ("ninth harmonic " << db << " dB");
    CHECK (db < -40.0);
}

namespace
{
    // Task 0014: what a broken host or plugin before us can send
    const std::array<float, 6> badSamples { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 3e38f, -3e38f, std::numeric_limits<float>::max() };

    bool allFinite (const std::vector<float>& samples)
    {
        return std::all_of (samples.begin(), samples.end(), [] (float s) { return std::isfinite (s); });
    }

    void processInBlocks (OversampledFilterChain& chain, std::vector<float>& left, std::vector<float>* right = nullptr)
    {
        const int total = static_cast<int> (left.size());
        for (int start = 0; start < total; start += 512)
        {
            const auto length = static_cast<size_t> (std::min (512, total - start));
            const auto offset = static_cast<size_t> (start);
            std::array<std::span<float>, 2> channels { std::span<float> { left }.subspan (offset, length), {} };
            if (right != nullptr)
                channels[1] = std::span<float> { *right }.subspan (offset, length);
            chain.process ({ channels.data(), right != nullptr ? 2u : 1u }, static_cast<int> (length));
        }
    }
}

TEST_CASE ("One bad input sample leaves every output sample finite", "[dsp][oversampling][non-finite]")
{
    // .claude/rules/dsp.md, task 0014. Every filter variant, every factor, resonance 100 %, Drive 0 and
    // 24 dB, noise with one bad sample at 1000. Red before the guard: non-finite to the end.
    const auto variants = moodz::dsp::filterVariants();
    const auto input = noise (2048, 89u);
    for (std::size_t variant = 0; variant < variants.size(); ++variant)
        for (int factor : { off, twice, fourTimes })
        {
            auto chain = preparedChain (factor, FilterChoice { variant, 0.5f }, 1000.0f, 1.0f);
            for (float driveDb : { 0.0f, 24.0f })
                for (float bad : badSamples)
                {
                    INFO ("variant " << variant << ", factor index " << factor << ", Drive " << driveDb << ", bad sample " << bad);
                    chain->setDriveDb (driveDb);
                    chain->reset();
                    auto samples = input;
                    samples[1000] = bad;
                    processInBlocks (*chain, samples);
                    CHECK (allFinite (samples));
                }
        }
}

TEST_CASE ("A signal held at the bound keeps every filter finite", "[dsp][oversampling][non-finite]")
{
    // Code review of task 0014: the bound has to hold beyond one sample at one cutoff. Every variant at
    // 4x, 7 kHz square at +/-256 (the largest signal the guard lets through), resonance 100 %, cutoff
    // 20 Hz and 20 kHz, Drive 0 dB and 24 dB Asymmetric, at 44.1 and 192 kHz.
    const auto variants = moodz::dsp::filterVariants();
    for (double rate : { 44100.0, 192000.0 })
        for (std::size_t variant = 0; variant < variants.size(); ++variant)
            for (float cutoff : { 20.0f, 20000.0f })
                for (float driveDb : { 0.0f, 24.0f })
                {
                    INFO ("rate " << rate << ", variant " << variant << ", cutoff " << cutoff << ", Drive " << driveDb);
                    auto chain = std::make_unique<OversampledFilterChain>();
                    chain->setCutoffHz (cutoff);
                    chain->setResonance (1.0f);
                    chain->setFactorIndex (fourTimes);
                    chain->select (FilterChoice { variant, 0.5f });
                    chain->setDriveDb (driveDb);
                    chain->setDriveCurveIndex (asymmetricCurve);
                    chain->prepare (rate, 512);
                    chain->reset();
                    std::vector<float> samples (2048);
                    for (std::size_t i = 0; i < samples.size(); ++i)
                        samples[i] = (static_cast<int> (static_cast<double> (i) * 7000.0 * 2.0 / rate) % 2 == 0) ? 256.0f : -256.0f;
                    processInBlocks (*chain, samples);
                    CHECK (allFinite (samples));
                }
}

TEST_CASE ("The chain plays on after a bad input sample", "[dsp][oversampling][non-finite]")
{
    // Task 0014. 440 Hz sine at -12 dBFS, cutoff 1 kHz, resonance 20 %, one bad sample at 4800: every
    // sample finite, and 100 ms later the output is back to the one without it (run: 0). largestDifference
    // alone would not do: std::max skips a NaN. Largest difference printed by:
    // ./Builds/DspTests "The chain plays on after a bad input sample" -s | grep "difference"
    const auto input = sineAt (440.0, 0.25f, 19200);
    auto clean = input;
    auto cleanChain = preparedChain (twice, cytomicLowPass, 1000.0f, 0.2f);
    processInBlocks (*cleanChain, clean);
    for (float bad : badSamples)
    {
        auto samples = input;
        samples[4800] = bad;
        auto chain = preparedChain (twice, cytomicLowPass, 1000.0f, 0.2f);
        processInBlocks (*chain, samples);
        CHECK (allFinite (samples));
        const float difference = largestDifference (samples, clean, 4800 + 4800, samples.size());
        SUCCEED ("largest difference after a bad sample " << bad << ": " << difference);
        CHECK (difference < 1e-4f);
    }
}

TEST_CASE ("A bad sample counts as 0", "[dsp][oversampling][non-finite]")
{
    // Code review of task 0014: clamped to the bound, a huge sample came out as a click of about +48 dBFS.
    // NaN, infinities and anything beyond +/-256 are not a signal: same output as a 0, checked finite too,
    // since largestDifference skips a NaN. The last check shows a sample at the bound does reach the output.
    const auto input = noise (4096, 101u);
    auto rendered = [&] (float sample) {
        auto samples = input;
        samples[1000] = sample;
        auto chain = preparedChain (twice, cytomicHighPass, 1000.0f, 0.5f);
        processInBlocks (*chain, samples);
        return samples;
    };
    const auto zero = rendered (0.0f);
    for (float bad : badSamples)
    {
        INFO ("bad sample " << bad);
        const auto output = rendered (bad);
        CHECK (allFinite (output));
        CHECK (largestDifference (output, zero, 0, zero.size()) < 1e-6f);
    }
    const auto justAbove = rendered (256.5f);
    CHECK (largestDifference (justAbove, zero, 0, zero.size()) < 1e-6f);
    CHECK (largestDifference (rendered (256.0f), zero, 0, zero.size()) > 1.0f);
}

TEST_CASE ("A bad sample on one channel leaves the other alone", "[dsp][oversampling][non-finite]")
{
    // Task 0014, bad sample on the left only. The right channel is compared sample by sample and must
    // also be finite, since largestDifference skips a NaN.
    const auto input = noise (4096, 97u);
    auto cleanChain = preparedChain (twice, cytomicLowPass, 1000.0f, 0.5f);
    auto cleanLeft = input, cleanRight = input;
    processInBlocks (*cleanChain, cleanLeft, &cleanRight);
    for (float bad : badSamples)
    {
        auto chain = preparedChain (twice, cytomicLowPass, 1000.0f, 0.5f);
        auto left = input, right = input;
        left[1000] = bad;
        processInBlocks (*chain, left, &right);
        CHECK (allFinite (right));
        CHECK (largestDifference (right, cleanRight, 0, right.size()) < 1e-6f);
    }
}

TEST_CASE ("A hot signal within the bound is not touched", "[dsp][oversampling][non-finite]")
{
    // Task 0014: at Off the chain is the filter and a delay, so a signal at 255.9 and the same signal at
    // 1/256 give outputs in that ratio; a lower bound would break it. Cytomic SVF is linear.
    const auto input = sineAt (440.0, 1.0f, 9600);
    auto scaled = [&] (float gain) {
        auto samples = input;
        for (auto& s : samples)
            s *= gain;
        auto chain = preparedChain (off, cytomicLowPass, 1000.0f, 0.2f);
        processInBlocks (*chain, samples);
        for (auto& s : samples)
            s /= gain;
        return samples;
    };
    const auto low = scaled (1.0f / 256.0f), high = scaled (255.9f);
    CHECK (allFinite (high));
    CHECK (largestDifference (low, high, 0, low.size()) < 1e-3f);
}
