#include "dsp/CrossfadingFilterBank.h"
#include "dsp/FilterModels.h"
#include "dsp/MultimodeFilter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
            int length = std::min (blockSize, total - start);
            if (next < changes.size() && start < changes[next].at && changes[next].at < start + length)
                length = changes[next].at - start;
            if (next < changes.size() && start == changes[next].at)
                bank.select (changes[next++].choice);
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
    // Change to Cytomic HP at 4096, to Vintage Huovilainen at 4608 while the first crossfade runs
    // (until 5056). The second one starts at the first block after that, 5120, and ends at
    // 5120 + 960: from there the output is Vintage Huovilainen alone, reset at 5120.
    const auto input = noise (16384, 41u);
    const auto output = renderBank (cytomicLowPass, { { 4096, cytomicHighPass }, { 4608, vintageHuov } }, input, 512);
    const auto last = renderAloneFrom (vintageHuov, input, 5120);
    for (auto s : output)
        REQUIRE (std::isfinite (s));
    CHECK (largestDifference (output, last, 5120 + crossfadeLength, output.size()) < 1e-6f);
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
