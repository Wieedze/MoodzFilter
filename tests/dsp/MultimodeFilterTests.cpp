#include "dsp/FilterModels.h"
#include "dsp/MultimodeFilter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

using moodz::dsp::FilterSetup;
using moodz::dsp::filterSetups;
using moodz::dsp::MultimodeFilter;
namespace sfpp = sst::filtersplusplus;

namespace
{
    constexpr double sampleRate = 48000.0;

    std::vector<float> sine (double frequency, int length)
    {
        std::vector<float> samples (static_cast<size_t> (length));
        for (int i = 0; i < length; ++i)
            samples[static_cast<size_t> (i)] = 0.25f * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * i / sampleRate));
        return samples;
    }

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

    double rms (const std::vector<float>& samples, size_t from)
    {
        double sum = 0.0;
        for (size_t i = from; i < samples.size(); ++i)
            sum += static_cast<double> (samples[i]) * samples[i];
        return std::sqrt (sum / static_cast<double> (samples.size() - from));
    }

    MultimodeFilter preparedFilter (const FilterSetup& setup, float cutoffHz, float resonance)
    {
        MultimodeFilter filter;
        REQUIRE (filter.configure (setup));
        filter.prepare (sampleRate, 512);
        filter.setCutoffHz (cutoffHz);
        filter.setResonance (resonance);
        return filter;
    }

    void processMono (MultimodeFilter& filter, std::vector<float>& samples)
    {
        const std::array<std::span<float>, 1> channels { std::span<float> { samples } };
        filter.process (channels, static_cast<int> (samples.size()));
    }

    // Gain in dB of the filter on a sine, measured in steady state (after 100 ms)
    double steadyStateGainDb (const FilterSetup& setup, double frequency)
    {
        auto filter = preparedFilter (setup, 1000.0f, 0.0f);
        const auto input = sine (frequency, 9600);
        auto output = input;
        processMono (filter, output);
        return 20.0 * std::log10 (rms (output, 4800) / rms (input, 4800));
    }

    const FilterSetup cytomicLowPass { sfpp::FilterModel::CytomicSVF, { sfpp::Passband::LP }, "cytomic-svf-lp", "Cytomic SVF LP" };
    const FilterSetup cytomicHighPass { sfpp::FilterModel::CytomicSVF, { sfpp::Passband::HP }, "cytomic-svf-hp", "Cytomic SVF HP" };
}

TEST_CASE ("MultimodeFilter low pass response", "[dsp][filter]")
{
    // 2-pole low pass, cutoff 1 kHz: |H| ~ 1 a decade below, (1/10)^2 = -40 dB a decade above
    CHECK (steadyStateGainDb (cytomicLowPass, 100.0) == Catch::Approx (0.0).margin (1.0));
    const auto above = steadyStateGainDb (cytomicLowPass, 10000.0);
    INFO ("10 kHz gain " << above << " dB");
    CHECK (above < -34.0);
    CHECK (above > -46.0);
}

TEST_CASE ("MultimodeFilter high pass response", "[dsp][filter]")
{
    // Mirror of the low pass
    CHECK (steadyStateGainDb (cytomicHighPass, 10000.0) == Catch::Approx (0.0).margin (1.0));
    const auto below = steadyStateGainDb (cytomicHighPass, 100.0);
    INFO ("100 Hz gain " << below << " dB");
    CHECK (below < -34.0);
    CHECK (below > -46.0);
}

TEST_CASE ("Every filter setup is a valid configuration", "[dsp][filter]")
{
    for (const auto& candidate : filterSetups())
    {
        INFO (candidate.name);
        MultimodeFilter filter;
        CHECK (filter.configure (candidate));
    }
}

TEST_CASE ("Every filter setup stays finite at the bounds", "[dsp][filter]")
{
    // .claude/rules/dsp.md: finite output. Peak bound 4 (+24 dB over the 0.25 input) catches a
    // runaway; the largest peak measured on this signal is 2.59 (Cytomic SVF, 96 kHz, cutoff
    // 20 kHz, resonance 1), printed by: ./Builds/DspTests "Every filter setup stays finite at the bounds" -s | grep "largest peak"
    float largestPeak = 0.0f;
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto input = noise (static_cast<int> (rate / 2), 7u);
        for (const auto& candidate : filterSetups())
        {
            for (float cutoff : { 20.0f, 1000.0f, 20000.0f })
            {
                for (float resonance : { 0.0f, 0.5f, 1.0f })
                {
                    INFO (candidate.name << " at " << rate << " Hz, cutoff " << cutoff << ", resonance " << resonance);
                    MultimodeFilter filter;
                    REQUIRE (filter.configure (candidate));
                    filter.prepare (rate, 512);
                    filter.setCutoffHz (cutoff);
                    filter.setResonance (resonance);
                    auto output = input;
                    processMono (filter, output);
                    float peak = 0.0f;
                    bool finite = true;
                    for (auto s : output)
                    {
                        finite = finite && std::isfinite (s);
                        peak = std::max (peak, std::abs (s));
                    }
                    REQUIRE (finite);
                    CHECK (peak < 4.0f);
                    largestPeak = std::max (largestPeak, peak);
                }
            }
        }
    }
    SUCCEED ("largest peak " << largestPeak);
}

TEST_CASE ("MultimodeFilter stays finite when the sample rate drops", "[dsp][filter]")
{
    // rt-safety review of task 0007: a cutoff set at 48 kHz must not exceed Nyquist after
    // prepare() at a lower rate (OB-Xd models computed tan(pi * f / sr) past pi / 2)
    const auto input = noise (32000, 13u);
    for (const auto& candidate : filterSetups())
    {
        INFO (candidate.name);
        MultimodeFilter filter;
        REQUIRE (filter.configure (candidate));
        filter.prepare (48000.0, 512);
        filter.setCutoffHz (20000.0f);
        filter.setResonance (1.0f);
        filter.prepare (32000.0, 512);
        auto output = input;
        processMono (filter, output);
        for (auto s : output)
            REQUIRE (std::isfinite (s));
    }
}

TEST_CASE ("MultimodeFilter does not depend on the block size", "[dsp][filter]")
{
    // Cutoff from 300 Hz to 5 kHz exactly at sample 4096 (a packet boundary), whatever the block size
    constexpr int changeAt = 4096;
    const auto input = noise (8192, 11u);
    const int total = static_cast<int> (input.size());

    for (const auto& candidate : filterSetups())
    {
        auto render = [&] (int blockSize) {
            auto filter = preparedFilter (candidate, 300.0f, 0.7f);
            auto output = input;
            int start = 0;
            while (start < total)
            {
                int length = std::min (blockSize, total - start);
                if (start < changeAt && changeAt < start + length)
                    length = changeAt - start;
                if (start == changeAt)
                    filter.setCutoffHz (5000.0f);
                const std::array<std::span<float>, 1> channels { std::span<float> { output }.subspan (static_cast<size_t> (start)) };
                filter.process (channels, length);
                start += length;
            }
            return output;
        };

        const auto reference = render (total);
        for (int blockSize : { 1, 7, 64, 480, 512 })
        {
            INFO (candidate.name << " block size " << blockSize);
            const auto output = render (blockSize);
            for (size_t i = 0; i < output.size(); ++i)
                REQUIRE (output[i] == Catch::Approx (reference[i]).margin (1e-6));
        }
    }
}

TEST_CASE ("MultimodeFilter reset clears the state", "[dsp][filter]")
{
    const auto input = noise (4096, 3u);
    for (const auto& candidate : filterSetups())
    {
        INFO (candidate.name);
        auto filter = preparedFilter (candidate, 800.0f, 0.6f);
        auto first = input;
        processMono (filter, first);
        filter.reset();
        auto second = input;
        processMono (filter, second);
        REQUIRE (first == second);
    }
}

TEST_CASE ("MultimodeFilter reset in the middle of a packet clears the state", "[dsp][filter]")
{
    const auto input = noise (4096, 17u);
    for (const auto& candidate : filterSetups())
    {
        INFO (candidate.name);
        auto used = preparedFilter (candidate, 800.0f, 0.6f);
        std::vector<float> partial (7, 0.1f); // 7 samples: stops inside a 16-sample packet
        processMono (used, partial);
        used.reset();
        auto fresh = preparedFilter (candidate, 800.0f, 0.6f);

        auto a = input;
        auto b = input;
        processMono (used, a);
        processMono (fresh, b);
        REQUIRE (a == b);
    }
}

TEST_CASE ("MultimodeFilter never reads past its channel spans", "[dsp][filter]")
{
    // numSamples larger than the spans: only the samples the spans hold are processed
    auto filter = preparedFilter (cytomicLowPass, 1000.0f, 0.0f);
    std::vector<float> storage (64, 0.25f);
    const std::array<std::span<float>, 1> channels { std::span<float> { storage }.first (32) };
    filter.process (channels, 64);
    for (size_t i = 32; i < storage.size(); ++i)
        REQUIRE (storage[i] == 0.25f);
}

TEST_CASE ("MultimodeFilter handles a block of 0 samples", "[dsp][filter]")
{
    const auto input = noise (512, 5u);
    auto withEmptyBlock = preparedFilter (cytomicLowPass, 1000.0f, 0.3f);
    auto without = preparedFilter (cytomicLowPass, 1000.0f, 0.3f);

    auto a = input;
    auto b = input;
    std::vector<float> empty;
    processMono (withEmptyBlock, empty);
    processMono (withEmptyBlock, a);
    processMono (without, b);
    REQUIRE (a == b);
}

TEST_CASE ("MultimodeFilter processes linked channels identically", "[dsp][filter]")
{
    for (const auto& candidate : filterSetups())
    {
        INFO (candidate.name);
        auto filter = preparedFilter (candidate, 1500.0f, 0.5f);
        auto left = noise (2048, 9u);
        auto right = left;
        const std::array<std::span<float>, 2> channels { std::span<float> { left }, std::span<float> { right } };
        filter.process (channels, 2048);
        REQUIRE (left == right);
    }
}

TEST_CASE ("The filter setups are the supported model and passband pairs", "[dsp][filter]")
{
    // ADR 0006: Cytomic SVF plays 4 passbands, K35 low and high pass, the others low pass only;
    // passbands listed with Filter::availableModelConfigurations at sst-filters 87a171b
    using sfpp::FilterModel;
    using sfpp::Passband;
    const std::array<std::pair<FilterModel, Passband>, 9> expected { {
        { FilterModel::CytomicSVF, Passband::LP },
        { FilterModel::CytomicSVF, Passband::HP },
        { FilterModel::CytomicSVF, Passband::BP },
        { FilterModel::CytomicSVF, Passband::Notch },
        { FilterModel::VintageLadder, Passband::LP },
        { FilterModel::OBXD_4Pole, Passband::LP },
        { FilterModel::DiodeLadder, Passband::LP },
        { FilterModel::K35, Passband::LP },
        { FilterModel::K35, Passband::HP },
    } };
    const auto setups = filterSetups();
    REQUIRE (setups.size() == expected.size());
    for (size_t i = 0; i < setups.size(); ++i)
    {
        INFO (setups[i].name);
        CHECK (setups[i].model == expected[i].first);
        CHECK (setups[i].config.pt == expected[i].second);
    }
}
