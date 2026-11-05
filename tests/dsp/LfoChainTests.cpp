#include "dsp/FilterModels.h"
#include "dsp/OversampledFilterChain.h"
#include "dsp/SyncedLfo.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>

using Catch::Approx;
using moodz::dsp::FilterChoice;
using moodz::dsp::OversampledFilterChain;
using moodz::dsp::SyncedLfo;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int off = 0, twice = 1, fourTimes = 2;
    constexpr int sine = 0, sawtooth = 2, square = 3;
    constexpr FilterChoice cytomicLowPass { 0, 0.0f };

    struct Settings
    {
        int factor = twice;
        float cutoff = 1000.0f;
        float resonance = 0.2f;
        int shape = sine;
        int rate = SyncedLfo::defaultRateIndex;
        float depth = 0.0f;
        double tempo = 120.0;
    };

    std::unique_ptr<OversampledFilterChain> preparedChain (const Settings& s)
    {
        auto chain = std::make_unique<OversampledFilterChain>();
        chain->setCutoffHz (s.cutoff);
        chain->setResonance (s.resonance);
        chain->setFactorIndex (s.factor);
        chain->select (cytomicLowPass);
        chain->setLfoShapeIndex (s.shape);
        chain->setLfoRateIndex (s.rate);
        chain->setLfoDepthOctaves (s.depth);
        chain->setTempoBpm (s.tempo);
        chain->prepare (sampleRate, 512);
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

    std::vector<float> sineAt (double frequency, float peak, int length)
    {
        std::vector<float> samples (static_cast<std::size_t> (length));
        for (int i = 0; i < length; ++i)
            samples[static_cast<std::size_t> (i)] = peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * i / sampleRate));
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

    double rmsDb (const std::vector<float>& samples, std::size_t centre, std::size_t halfWidth)
    {
        double sum = 0.0;
        for (std::size_t i = centre - halfWidth; i < centre + halfWidth; ++i)
            sum += static_cast<double> (samples[i]) * samples[i];
        return 10.0 * std::log10 (sum / static_cast<double> (2 * halfWidth));
    }

    double octavesBetween (float a, float b)
    {
        return std::abs (std::log2 (static_cast<double> (a) / static_cast<double> (b)));
    }
}

TEST_CASE ("At depth 0 the LFO changes nothing", "[dsp][lfo]")
{
    // Goal of task 0015, against a chain left at its defaults: same output for any shape and rate,
    // and the cutoff is the parameter itself
    const auto input = noise (8192, 103u);
    auto reference = input;
    auto plain = std::make_unique<OversampledFilterChain>();
    plain->setCutoffHz (1000.0f);
    plain->setResonance (0.2f);
    plain->setFactorIndex (twice);
    plain->select (cytomicLowPass);
    plain->prepare (sampleRate, 512);
    plain->reset();
    run (*plain, reference);
    for (int shape : { sine, sawtooth, square })
        for (int rate : { 0, 12, 21 })
        {
            INFO ("shape " << shape << ", rate " << rate);
            auto chain = preparedChain ({ .shape = shape, .rate = rate });
            auto output = input;
            run (*chain, output);
            CHECK (allFinite (output));
            CHECK (largestDifference (output, reference, 0, output.size()) < 1e-7f);
            CHECK (chain->modulatedCutoffHz() == Approx (1000.0f).margin (1e-3));
        }
}

TEST_CASE ("The LFO moves the cutoff in time", "[dsp][lfo]")
{
    // SPEC. 2 kHz sine at -12 dBFS, Cytomic SVF Low-pass at 500 Hz, sine LFO on a 1/4 at 120 BPM (0.5 s),
    // depth +2 octaves: the cutoff reaches 2 kHz at phase 1/4 (sample 6000 + 24000 n) and 125 Hz at
    // phase 3/4 (18000 + 24000 n). Negative depth swaps them. Printed by:
    // ./Builds/DspTests "The LFO moves the cutoff in time" -s | grep "swing"
    const auto input = sineAt (2000.0, 0.25f, 72000);
    auto swingDb = [&] (float depth) {
        auto chain = preparedChain ({ .cutoff = 500.0f, .depth = depth });
        auto output = input;
        run (*chain, output);
        REQUIRE (allFinite (output));
        return rmsDb (output, 30000, 480) - rmsDb (output, 42000, 480);
    };
    const double up = swingDb (2.0f), down = swingDb (-2.0f);
    SUCCEED ("swing +2 octaves " << up << " dB, -2 octaves " << down << " dB");
    CHECK (up > 20.0);
    CHECK (down < -20.0);

    // A period of 0.5 s: the second cycle repeats in the third
    auto chain = preparedChain ({ .cutoff = 500.0f, .depth = 2.0f });
    auto output = input;
    run (*chain, output);
    double largest = 0.0;
    for (std::size_t i = 24000; i < 48000; ++i)
        largest = std::max (largest, std::abs (static_cast<double> (output[i]) - output[i + 24000]));
    CHECK (largest < 1e-3);
}

TEST_CASE ("The cutoff follows depth and tempo", "[dsp][lfo]")
{
    // Square on 4 bars, phase in its first half: value +1, so the cutoff is 1 kHz x 2^depth. The
    // formula from the decision notes of task 0015, read on the modulation grid.
    for (float depth : { 4.0f, 2.0f, 1.0f, -1.0f, -3.0f })
    {
        INFO ("depth " << depth);
        auto chain = preparedChain ({ .shape = square, .rate = 0, .depth = depth });
        std::vector<float> silence (4096, 0.0f);
        run (*chain, silence);
        CHECK (chain->modulatedCutoffHz() == Approx (1000.0f * std::pow (2.0f, depth)).epsilon (1e-3));
    }
    // Beyond +/-4 octaves the depth is bounded
    auto chain = preparedChain ({ .shape = square, .rate = 0, .depth = 9.0f });
    std::vector<float> silence (4096, 0.0f);
    run (*chain, silence);
    CHECK (chain->modulatedCutoffHz() == Approx (16000.0f).epsilon (1e-3));

    // Tempo 240 BPM: a 1/4 lasts 12000 samples, so at sample 6000 a square is in its second half
    auto faster = preparedChain ({ .shape = square, .depth = 1.0f, .tempo = 240.0 });
    std::vector<float> samples (7200, 0.0f);
    run (*faster, samples);
    CHECK (faster->modulatedCutoffHz() == Approx (500.0f).epsilon (1e-2));
}

TEST_CASE ("The LFO stays finite with the cutoff at a bound", "[dsp][lfo]")
{
    // .claude/rules/dsp.md: the formula can reach 20 kHz x 16 and 20 Hz / 16; the filter clamps them.
    const auto input = noise (8192, 107u);
    for (float cutoff : { 20.0f, 20000.0f })
        for (float depth : { 4.0f, -4.0f })
            for (int factor : { off, fourTimes })
                for (int shape : { sine, square })
                {
                    INFO ("cutoff " << cutoff << ", depth " << depth << ", factor index " << factor << ", shape " << shape);
                    auto chain = preparedChain ({ .factor = factor, .cutoff = cutoff, .resonance = 1.0f, .shape = shape, .rate = 21, .depth = depth });
                    auto output = input;
                    run (*chain, output);
                    CHECK (allFinite (output));
                }
}

TEST_CASE ("The chain with the LFO does not depend on the block size", "[dsp][lfo]")
{
    // .claude/rules/dsp.md, ROADMAP. Every shape, fast rate, depth 3 octaves, and a depth change at 4096
    const auto input = noise (16384, 109u);
    for (int shape : { 0, 1, 2, 3 })
    {
        auto renderAt = [&] (int blockSize) {
            auto samples = input;
            auto chain = preparedChain ({ .shape = shape, .rate = 18, .depth = 3.0f });
            std::span<float> all { samples };
            run (*chain, all.first (4096), blockSize);
            chain->setLfoDepthOctaves (-2.0f);
            run (*chain, all.subspan (4096), blockSize);
            return samples;
        };
        const auto reference = renderAt (4096);
        CHECK (allFinite (reference));
        for (int blockSize : { 1, 7, 64, 480, 512, 2048 })
        {
            INFO ("shape " << shape << ", block size " << blockSize);
            CHECK (largestDifference (renderAt (blockSize), reference, 0, reference.size()) < 1e-6f);
        }
    }
}

TEST_CASE ("The cutoff moves only on the modulation grid", "[dsp][lfo]")
{
    // Task 0015: every 32 base samples, counted across blocks, whatever the block
    auto chain = preparedChain ({ .shape = sine, .rate = 21, .depth = 2.0f });
    std::array<float, 1> sample { 0.0f };
    const std::array<std::span<float>, 1> channels { std::span<float> { sample } };
    float previous = chain->modulatedCutoffHz();
    int changes = 0;
    for (int i = 0; i < 3200; ++i)
    {
        chain->process (channels, 1);
        const float now = chain->modulatedCutoffHz();
        if (i % 32 != 0)
            REQUIRE (std::abs (now - previous) < 1e-6f);
        else if (std::abs (now - previous) > 1e-6f)
            ++changes;
        previous = now;
    }
    CHECK (changes > 90);
}

TEST_CASE ("A depth change ramps over 20 ms", "[dsp][lfo]")
{
    // SPEC: depth smoothed. Square on 4 bars, value +1 and steady. Depth 0 to 2 octaves at sample 3200:
    // half way in octaves after 480 samples, and at the target from 960 samples on, within the grid's 32.
    auto chain = preparedChain ({ .shape = square, .rate = 0 });
    std::vector<float> silence (3200, 0.0f);
    run (*chain, silence);
    CHECK (chain->modulatedCutoffHz() == Approx (1000.0f).margin (1e-3));
    chain->setLfoDepthOctaves (2.0f);
    silence.assign (480, 0.0f);
    run (*chain, silence);
    CHECK (std::log2 (chain->modulatedCutoffHz() / 1000.0f) == Approx (1.0f).margin (0.1f));
    silence.assign (416, 0.0f); // 896 after the change, more than one grid step before the end
    run (*chain, silence);
    CHECK (std::log2 (chain->modulatedCutoffHz() / 1000.0f) < 1.97f);
    silence.assign (96, 0.0f); // 992: past the end by the grid's 32
    run (*chain, silence);
    CHECK (std::log2 (chain->modulatedCutoffHz() / 1000.0f) == Approx (2.0f).margin (1e-4));
}

TEST_CASE ("A square edge is a short curve, not a jump", "[dsp][lfo]")
{
    // Decision notes of task 0015: 2 ms one-pole on the value. Depth 4 octaves, an edge is 8 octaves:
    // no grid step moves more than 1 - exp (-32 / (0.002 x 48000)) = 0.283 of it, 2.27 octaves, and 10 ms
    // later the cutoff is at its target.
    auto chain = preparedChain ({ .shape = square, .depth = 4.0f, .tempo = 120.0 });
    std::vector<float> silence (11968, 0.0f); // the edge is at 12000
    run (*chain, silence);
    CHECK (chain->modulatedCutoffHz() == Approx (16000.0f).epsilon (1e-3));
    float previous = chain->modulatedCutoffHz();
    double largest = 0.0;
    for (int step = 0; step < 16; ++step)
    {
        silence.assign (32, 0.0f);
        run (*chain, silence);
        largest = std::max (largest, octavesBetween (chain->modulatedCutoffHz(), previous));
        previous = chain->modulatedCutoffHz();
    }
    SUCCEED ("largest step " << largest << " octaves");
    CHECK (largest < 2.3);
    CHECK (largest > 2.2);
    silence.assign (480, 0.0f);
    run (*chain, silence);
    CHECK (chain->modulatedCutoffHz() == Approx (62.5f).epsilon (0.05));
}

TEST_CASE ("The song position sets the LFO", "[dsp][lfo]")
{
    // Decision notes: square on a 1/2, position 1 quarter note is its second half, whatever came before
    auto chain = preparedChain ({ .shape = square, .rate = 9, .depth = 1.0f });
    std::vector<float> silence (2048, 0.0f);
    chain->syncLfoToPosition (1.0);
    run (*chain, silence);
    CHECK (chain->modulatedCutoffHz() == Approx (500.0f).epsilon (1e-2));
    chain->syncLfoToPosition (0.0);
    run (*chain, silence);
    CHECK (chain->modulatedCutoffHz() == Approx (2000.0f).epsilon (1e-2));
}

TEST_CASE ("LFO settings reach both paths during a factor crossfade", "[dsp][lfo]")
{
    // A factor change with the LFO moving: finite, and the cutoff keeps following
    const auto input = noise (19200, 113u);
    auto chain = preparedChain ({ .shape = sawtooth, .rate = 18, .depth = 3.0f });
    auto output = input;
    std::span<float> all { output };
    run (*chain, all.first (4800));
    chain->setFactorIndex (fourTimes);
    run (*chain, all.subspan (4800));
    CHECK (allFinite (output));
    const auto unchanged = [&] {
        auto reference = preparedChain ({ .factor = fourTimes, .shape = sawtooth, .rate = 18, .depth = 3.0f });
        auto samples = input;
        run (*reference, samples);
        return samples;
    }();
    // Past the crossfade the path at 4x plays the same LFO as a chain at 4x from the start
    CHECK (largestDifference (output, unchanged, 14400, output.size()) < 1e-2f);
}
