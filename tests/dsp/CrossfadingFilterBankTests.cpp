#include "dsp/CrossfadingFilterBank.h"
#include "dsp/FilterModels.h"
#include "dsp/MultimodeFilter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using moodz::dsp::CrossfadingFilterBank;
using moodz::dsp::FilterChoice;
using moodz::dsp::filterVariants;
using moodz::dsp::MultimodeFilter;

namespace
{
    constexpr double sampleRate = 48000.0;
    // 20 ms at 48 kHz (task 0011, same ramp time as Gain out)
    constexpr int crossfadeLength = 960;

    // Variant indices in filterVariants() (ADR 0008)
    constexpr FilterChoice cytomicLowPass { 0, 0.0f }, cytomicHighPass { 1, 0.0f }, vintageHuov { 6, 0.0f }, obxdMorph { 10, 0.5f };

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

    void preparedBank (CrossfadingFilterBank& bank, FilterChoice choice)
    {
        bank.prepare (sampleRate, 512);
        bank.setCutoffHz (1000.0f);
        bank.setResonance (0.5f);
        bank.select (choice);
        bank.reset();
    }

    // One change of choice per entry: { sample, choice }, applied at the start of the block that
    // begins at that sample. Blocks of blockSize, split so every change starts a block.
    struct Change
    {
        int at;
        FilterChoice choice;
    };

    std::vector<float> renderBank (FilterChoice first, const std::vector<Change>& changes, const std::vector<float>& input, int blockSize)
    {
        CrossfadingFilterBank bank;
        preparedBank (bank, first);
        auto output = input;
        const int total = static_cast<int> (input.size());
        int start = 0;
        size_t next = 0;
        while (start < total)
        {
            if (next < changes.size() && start == changes[next].at)
                bank.select (changes[next++].choice);
            int length = std::min (blockSize, total - start);
            if (next < changes.size() && changes[next].at < start + length)
                length = changes[next].at - start;
            const std::array<std::span<float>, 1> channels { std::span<float> { output }.subspan (static_cast<size_t> (start), static_cast<size_t> (length)) };
            bank.process (channels, length);
            start += length;
        }
        return output;
    }

    // The variant alone, reset at sample `from`, fed the input from there on (zeros before)
    std::vector<float> renderAloneFrom (FilterChoice choice, const std::vector<float>& input, int from)
    {
        MultimodeFilter filter;
        REQUIRE (filter.configure (filterVariants()[choice.variant]));
        filter.prepare (sampleRate, 512);
        filter.setCutoffHz (1000.0f);
        filter.setResonance (0.5f);
        filter.setExtra (choice.extra);
        std::vector<float> output (input.size(), 0.0f);
        std::copy (input.begin() + from, input.end(), output.begin() + from);
        const std::array<std::span<float>, 1> channels { std::span<float> { output }.subspan (static_cast<size_t> (from)) };
        filter.process (channels, static_cast<int> (output.size()) - from);
        return output;
    }

    float largestDifference (const std::vector<float>& a, const std::vector<float>& b, size_t from, size_t to)
    {
        float largest = 0.0f;
        for (size_t i = from; i < to; ++i)
            largest = std::max (largest, std::abs (a[i] - b[i]));
        return largest;
    }
}

TEST_CASE ("Without a change, the bank plays its variant alone", "[dsp][crossfade]")
{
    // Task 0011: no change means no crossfade, so the output is the variant's own output
    const auto input = noise (8192, 31u);
    for (auto choice : { cytomicLowPass, vintageHuov, obxdMorph })
    {
        INFO ("variant " << choice.variant);
        const auto output = renderBank (choice, {}, input, 512);
        const auto alone = renderAloneFrom (choice, input, 0);
        CHECK (largestDifference (output, alone, 0, output.size()) < 1e-6f);
    }
}

TEST_CASE ("A change crossfades to the new variant in 20 ms", "[dsp][crossfade]")
{
    // From the change on, the incoming variant starts clean (reset) and the outgoing one fades
    // out over 960 samples: from sample change + 960 the output is the incoming variant alone,
    // and one sample before it still holds some of the outgoing one.
    constexpr int change = 4096;
    const auto input = noise (8192, 37u);
    const auto output = renderBank (cytomicLowPass, { { change, cytomicHighPass } }, input, 512);
    const auto incoming = renderAloneFrom (cytomicHighPass, input, change);
    const auto outgoing = renderBank (cytomicLowPass, {}, input, 512);

    CHECK (largestDifference (output, outgoing, 0, change) < 1e-6f);
    CHECK (largestDifference (output, incoming, change + crossfadeLength, output.size()) < 1e-6f);
    CHECK (std::abs (output[change + crossfadeLength - 1] - incoming[change + crossfadeLength - 1]) > 1e-6f);
}

TEST_CASE ("A change during a crossfade waits for it to end", "[dsp][crossfade]")
{
    // Change to Cytomic HP at 4096, to Vintage Huovilainen at 4608 while the first crossfade runs.
    // The second one starts the sample the first one ends, 4096 + 960 = 5056, whatever the block
    // size (code review of task 0011), and ends at 5056 + 960: from there the output is Vintage
    // Huovilainen alone, reset at 5056.
    constexpr int secondStart = 4096 + crossfadeLength;
    const auto input = noise (16384, 41u);
    const auto last = renderAloneFrom (vintageHuov, input, secondStart);
    for (int blockSize : { 1, 7, 64, 512, 16384 })
    {
        INFO ("block size " << blockSize);
        const auto output = renderBank (cytomicLowPass, { { 4096, cytomicHighPass }, { 4608, vintageHuov } }, input, blockSize);
        for (auto s : output)
            REQUIRE (std::isfinite (s));
        CHECK (largestDifference (output, last, secondStart + crossfadeLength, output.size()) < 1e-6f);
    }
}

TEST_CASE ("A new extra value during a crossfade reaches the incoming variant", "[dsp][crossfade]")
{
    // Code review of task 0011: crossfade from Cytomic LP to OB-Xd Morph 0.5 at 4096, Morph moved
    // to 1.0 at 4352 while the crossfade runs. The incoming OB-Xd takes 1.0 at 4352, as it would
    // without a crossfade: after the crossfade the output is OB-Xd alone, reset at 4096, Morph 0.5
    // then 1.0 from 4352.
    constexpr int change = 4096, moved = 4352;
    const auto input = noise (8192, 61u);
    const auto output = renderBank (cytomicLowPass, { { change, obxdMorph }, { moved, FilterChoice { obxdMorph.variant, 1.0f } } }, input, 512);

    MultimodeFilter filter;
    REQUIRE (filter.configure (filterVariants()[obxdMorph.variant]));
    filter.prepare (sampleRate, 512);
    filter.setCutoffHz (1000.0f);
    filter.setResonance (0.5f);
    filter.setExtra (obxdMorph.extra);
    std::vector<float> expected (input.size(), 0.0f);
    std::copy (input.begin() + change, input.end(), expected.begin() + change);
    std::span<float> all { expected };
    const std::array<std::span<float>, 1> before { all.subspan (change, moved - change) };
    filter.process (before, moved - change);
    filter.setExtra (1.0f);
    const std::array<std::span<float>, 1> after { all.subspan (moved) };
    filter.process (after, static_cast<int> (expected.size()) - moved);
    CHECK (largestDifference (output, expected, change + crossfadeLength, output.size()) < 1e-6f);
}

TEST_CASE ("The crossfading bank does not depend on the block size", "[dsp][crossfade]")
{
    // .claude/rules/dsp.md: a change at sample 4096 gives the same output whatever the block size
    const auto input = noise (8192, 43u);
    const auto reference = renderBank (cytomicLowPass, { { 4096, obxdMorph } }, input, 8192);
    for (int blockSize : { 1, 7, 64, 480, 512 })
    {
        INFO ("block size " << blockSize);
        const auto output = renderBank (cytomicLowPass, { { 4096, obxdMorph } }, input, blockSize);
        CHECK (largestDifference (output, reference, 0, output.size()) < 1e-6f);
    }
}

TEST_CASE ("A new extra value on the same variant does not crossfade", "[dsp][crossfade]")
{
    // OB-Xd Morph moved: same variant, so the bank keeps playing it with the new extra value,
    // smoothed by sst-filters (task 0010), and does not reset it
    constexpr int change = 4096;
    const auto input = noise (8192, 47u);
    const auto output = renderBank (obxdMorph, { { change, FilterChoice { obxdMorph.variant, 1.0f } } }, input, 512);

    MultimodeFilter filter;
    REQUIRE (filter.configure (filterVariants()[obxdMorph.variant]));
    filter.prepare (sampleRate, 512);
    filter.setCutoffHz (1000.0f);
    filter.setResonance (0.5f);
    filter.setExtra (obxdMorph.extra);
    auto expected = input;
    std::span<float> all { expected };
    const std::array<std::span<float>, 1> before { all.first (change) };
    filter.process (before, change);
    filter.setExtra (1.0f);
    const std::array<std::span<float>, 1> after { all.subspan (change) };
    filter.process (after, static_cast<int> (expected.size()) - change);
    CHECK (largestDifference (output, expected, 0, output.size()) < 1e-6f);
}

TEST_CASE ("The crossfading bank handles a block of 0 samples and long blocks", "[dsp][crossfade]")
{
    // A block of 0 samples changes nothing; a block longer than the bank's internal scratch
    // (any size the host passes) is processed whole
    const auto input = noise (20000, 53u);
    const auto reference = renderBank (cytomicLowPass, { { 4096, cytomicHighPass } }, input, 512);
    CrossfadingFilterBank bank;
    preparedBank (bank, cytomicLowPass);
    auto output = input;
    std::span<float> all { output };
    std::vector<float> empty;
    const std::array<std::span<float>, 1> none { std::span<float> { empty } };
    bank.process (none, 0);
    const std::array<std::span<float>, 1> first { all.first (4096) };
    bank.process (first, 4096);
    bank.select (cytomicHighPass);
    const std::array<std::span<float>, 1> rest { all.subspan (4096) };
    bank.process (rest, static_cast<int> (output.size()) - 4096);
    CHECK (largestDifference (output, reference, 0, output.size()) < 1e-6f);
}

TEST_CASE ("A non-finite sample rate falls back to 48 kHz", "[dsp][crossfade]")
{
    // rt-safety review of task 0011: prepare let +inf through to the crossfade length (and the
    // filters). A host never sends it; the bank then behaves as at 48 kHz, crossfade included.
    const auto input = noise (8192, 59u);
    auto render = [&] (double rate) {
        CrossfadingFilterBank bank;
        bank.prepare (rate, 512);
        bank.setCutoffHz (1000.0f);
        bank.setResonance (0.5f);
        bank.select (cytomicLowPass);
        bank.reset();
        auto output = input;
        std::span<float> all { output };
        const std::array<std::span<float>, 1> first { all.first (4096) };
        bank.process (first, 4096);
        bank.select (cytomicHighPass);
        const std::array<std::span<float>, 1> rest { all.subspan (4096) };
        bank.process (rest, 4096);
        return output;
    };
    const auto reference = render (48000.0);
    for (double rate : { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), -1.0 })
    {
        INFO ("rate " << rate);
        const auto output = render (rate);
        CHECK (largestDifference (output, reference, 0, output.size()) < 1e-6f);
    }
}
