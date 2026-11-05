#include "dsp/FilterModels.h"
#include "dsp/OversampledFilterChain.h"
#include "dsp/SyncedLfo.h"

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
        double hostRate = sampleRate;
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
    for (int shape : { sine, 1, sawtooth, square })
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

TEST_CASE ("The filter holds its 20 Hz to 20 kHz range under the LFO", "[dsp][lfo]")
{
    // Task 0015: past a bound the cutoff stays at the bound, so the output equals the filter at the
    // bound without an LFO. Square on 4 bars, in its first half (+1) or second half (-1).
    const auto input = noise (8192, 127u);
    auto rendered = [&] (float cutoff, float depth, double position) {
        auto chain = preparedChain ({ .cutoff = cutoff, .shape = square, .rate = 0, .depth = depth });
        chain->syncLfoToPosition (position);
        auto output = input;
        run (*chain, output);
        CHECK (allFinite (output));
        return output;
    };
    CHECK (largestDifference (rendered (20000.0f, 4.0f, 0.0), rendered (20000.0f, 0.0f, 0.0), 0, input.size()) < 1e-6f);
    CHECK (largestDifference (rendered (20.0f, 4.0f, 8.0), rendered (20.0f, 0.0f, 0.0), 0, input.size()) < 1e-6f);
    // Within the range the LFO does move it: 500 Hz x 16 is 8 kHz, not the bound
    CHECK (largestDifference (rendered (500.0f, 4.0f, 0.0), rendered (20000.0f, 0.0f, 0.0), 0, input.size()) > 1e-3f);
}

TEST_CASE ("The LFO starts on the value of the song position", "[dsp][lfo]")
{
    // Code review of task 0015: after a prepare, playback from the middle of a cycle must not sweep from
    // the value of phase 0. Square on a 1/2, position 1 quarter note: -1 from the first sample, so the
    // cutoff is at 1 kHz / 16 at once, not coming down from 16 kHz.
    auto chain = preparedChain ({ .shape = square, .rate = 9, .depth = 4.0f });
    chain->syncLfoToPosition (1.0);
    std::array<float, 1> sample { 0.0f };
    const std::array<std::span<float>, 1> channels { std::span<float> { sample } };
    chain->process (channels, 1);
    CHECK (chain->modulatedCutoffHz() == Approx (62.5f).epsilon (1e-3));
}

TEST_CASE ("An extreme song position does not stop the LFO", "[dsp][lfo]")
{
    // rt-safety review of task 0015: on a rate under a quarter note, position / length overflows to
    // infinity from a finite position, which gave a NaN phase that stayed in the slew for good.
    constexpr int shortest = 21;
    for (double position : { std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest() })
    {
        INFO ("position " << position);
        // Sine: a square would read a NaN phase as its second half and hide it
        auto chain = preparedChain ({ .shape = sine, .rate = shortest, .depth = 1.0f });
        std::vector<float> silence (1024, 0.0f);
        run (*chain, silence);
        chain->syncLfoToPosition (position);
        run (*chain, silence);
        CHECK (std::isfinite (chain->modulatedCutoffHz()));
        chain->syncLfoToPosition (0.03125); // a quarter of a cycle: +1, and the next ticks reach it
        run (*chain, silence);
        CHECK (chain->modulatedCutoffHz() >= 500.0f);
        CHECK (chain->modulatedCutoffHz() <= 2000.0f);
    }
}

TEST_CASE ("At depth 0 a cutoff change applies at the next block", "[dsp][lfo]")
{
    // Code review of task 0015: at depth 0 the grid does not delay the cutoff, whatever the block
    auto chain = preparedChain ({});
    std::array<float, 7> samples {};
    const std::array<std::span<float>, 1> channels { std::span<float> { samples } };
    chain->process (channels, 7);
    chain->setCutoffHz (3000.0f);
    chain->process (channels, 7);
    CHECK (chain->modulatedCutoffHz() == Approx (3000.0f).margin (1e-3));
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
    silence.assign (448, 0.0f); // 928 after the change, one step of the grid before the end
    run (*chain, silence);
    CHECK (std::log2 (chain->modulatedCutoffHz() / 1000.0f) < 1.97f);
    silence.assign (64, 0.0f); // 992: past the end by the grid's 32
    run (*chain, silence);
    CHECK (std::log2 (chain->modulatedCutoffHz() / 1000.0f) == Approx (2.0f).margin (1e-4));
}

TEST_CASE ("A square edge is a short curve, not a jump", "[dsp][lfo]")
{
    // Decision notes of task 0015: 2 ms one-pole on the value. Depth 4 octaves, an edge is 8 octaves:
    // no grid step moves more than 0.283 of it, 2.27 octaves, and 10 ms later the cutoff is at its target:
    // python3 -c "from math import exp; c=1-exp(-32/(.002*48000)); print(c, 8*c)"
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

TEST_CASE ("The LFO keeps its times at another sample rate", "[dsp][lfo]")
{
    // Task 0015 verifier: everything else runs at 48 kHz. At 44.1 kHz, 120 BPM: a square on a 1/4 has its edge
    // at sample 11025; a step of the grid moves at most 0.304 of its 8 octaves, 2.43 octaves
    // (python3 -c "from math import exp; c=1-exp(-32/(.002*44100)); print(c, 8*c)"); a 20 ms depth ramp lasts
    // 882 samples, so 28 steps of the grid, 896 samples.
    auto chain = preparedChain ({ .shape = square, .depth = 4.0f, .hostRate = 44100.0 });
    std::vector<float> silence (11008, 0.0f); // the last step of the grid before the edge is at 10976
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
    CHECK (largest == Approx (2.43).margin (0.02));

    auto ramp = preparedChain ({ .shape = square, .rate = 0, .hostRate = 44100.0 });
    silence.assign (3200, 0.0f);
    run (*ramp, silence);
    ramp->setLfoDepthOctaves (2.0f);
    silence.assign (864, 0.0f); // 27 steps of the 28
    run (*ramp, silence);
    CHECK (std::log2 (ramp->modulatedCutoffHz() / 1000.0f) < 1.97f);
    silence.assign (64, 0.0f);
    run (*ramp, silence);
    CHECK (std::log2 (ramp->modulatedCutoffHz() / 1000.0f) == Approx (2.0f).margin (1e-4));
}

TEST_CASE ("Leaving rest starts from the LFO's value", "[dsp][lfo]")
{
    // Decision notes of task 0015: nothing follows the LFO at rest. Square on a 1/4, in its second half at
    // sample 18000, depth raised then: the cutoff goes down at once, not up from the old value.
    auto chain = preparedChain ({ .shape = square });
    std::vector<float> silence (18016, 0.0f);
    run (*chain, silence);
    chain->setLfoDepthOctaves (2.0f);
    silence.assign (64, 0.0f);
    run (*chain, silence);
    CHECK (chain->modulatedCutoffHz() < 1000.0f);
}

TEST_CASE ("A depth ramp to 0 lands on rest", "[dsp][lfo]")
{
    // Decision notes: exactly on the target, so the chain is at rest again and a cutoff change applies at
    // the next block, not on the grid. 2.3 octaves does not land on 0 by adding its steps.
    auto chain = preparedChain ({ .rate = 21, .depth = 2.3f });
    std::vector<float> silence (2048, 0.0f);
    run (*chain, silence);
    chain->setLfoDepthOctaves (0.0f);
    silence.assign (1056, 0.0f);
    run (*chain, silence);
    CHECK (chain->modulatedCutoffHz() == Approx (1000.0f).margin (1e-3));
    std::array<float, 7> samples {};
    const std::array<std::span<float>, 1> channels { std::span<float> { samples } };
    chain->process (channels, 7);
    chain->setCutoffHz (3000.0f);
    chain->process (channels, 7);
    CHECK (chain->modulatedCutoffHz() == Approx (3000.0f).margin (1e-3));
}

TEST_CASE ("A reset starts the LFO and its grid again", "[dsp][lfo]")
{
    // Grid and Slew notes. After samples that left the grid and the phase elsewhere, a reset gives what a
    // new chain gives: the grid from sample 0, phase 0, the slew on the LFO's value.
    auto chain = preparedChain ({ .shape = square, .rate = 9, .depth = 4.0f });
    std::vector<float> samples (26407, 0.0f); // second half of a 1/2, slew settled, 7 samples past a step of the grid
    run (*chain, samples);
    CHECK (chain->modulatedCutoffHz() == Approx (62.5f).epsilon (0.05));
    chain->reset();
    std::array<float, 1> sample { 0.0f };
    const std::array<std::span<float>, 1> channels { std::span<float> { sample } };
    chain->process (channels, 1);
    // Phase 0, value +1 at once
    CHECK (chain->modulatedCutoffHz() == Approx (16000.0f).epsilon (1e-3));

    // The grid restarts too: a sine moves the cutoff at samples 0, 32, 64..., whatever came before
    auto moving = preparedChain ({ .shape = sine, .rate = 21, .depth = 2.0f });
    samples.assign (7, 0.0f);
    run (*moving, samples);
    moving->reset();
    moving->process (channels, 1);
    float previous = moving->modulatedCutoffHz();
    for (int i = 1; i < 64; ++i)
    {
        moving->process (channels, 1);
        const float now = moving->modulatedCutoffHz();
        if (i % 32 != 0)
            REQUIRE (std::abs (now - previous) < 1e-6f);
        else
            CHECK (std::abs (now - previous) > 1e-3f);
        previous = now;
    }
}

TEST_CASE ("A first block of 0 samples does not start the LFO", "[dsp][lfo]")
{
    // .claude/rules/realtime-audio.md: the song position is known at the first real block
    auto chain = preparedChain ({ .shape = square, .rate = 9, .depth = 4.0f });
    std::array<float, 1> sample { 0.0f };
    const std::array<std::span<float>, 1> channels { std::span<float> { sample } };
    chain->process (channels, 0);
    chain->syncLfoToPosition (1.0);
    chain->process (channels, 1);
    CHECK (chain->modulatedCutoffHz() == Approx (62.5f).epsilon (1e-3));
}

TEST_CASE ("A non-finite depth is ignored", "[dsp][lfo]")
{
    // The header of OversampledFilterChain
    auto chain = preparedChain ({ .shape = square, .rate = 0, .depth = 1.0f });
    std::vector<float> silence (2048, 0.0f);
    run (*chain, silence);
    for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() })
    {
        INFO ("depth " << bad);
        chain->setLfoDepthOctaves (bad);
        run (*chain, silence);
        CHECK (chain->modulatedCutoffHz() == Approx (2000.0f).epsilon (1e-3));
    }
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
