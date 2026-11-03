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

using moodz::dsp::chooseFilter;
using moodz::dsp::filterSetups;
using moodz::dsp::filterVariants;
using moodz::dsp::MultimodeFilter;
namespace sfpp = sst::filtersplusplus;

namespace
{
    // Model entries, ADR 0007
    constexpr int vintageLadder = 4, obxd = 5, diodeLadder = 6, k35LowPass = 7, k35HighPass = 8;

    // Defaults of ADR 0008: Slope 24 dB, Vintage variant Runge-Kutta compensated, K35 saturation 25 %
    constexpr float defaultSlope = 24.0f;
    constexpr int defaultVintage = 1;
    constexpr float defaultSaturation = 0.25f;

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

    std::vector<float> render (const moodz::dsp::FilterSetup& setup, float extra, const std::vector<float>& input)
    {
        MultimodeFilter filter;
        REQUIRE (filter.configure (setup));
        filter.prepare (48000.0, 512);
        filter.setCutoffHz (1000.0f);
        filter.setResonance (0.5f);
        filter.setExtra (extra);
        auto output = input;
        const std::array<std::span<float>, 1> channels { std::span<float> { output } };
        filter.process (channels, static_cast<int> (output.size()));
        return output;
    }
}

TEST_CASE ("The filter variants are every model configuration the parameters reach", "[dsp][filter]")
{
    // ADR 0008: one prepared configuration per Model entry, plus one per Vintage Ladder sub-model
    // and per Diode Ladder slope step. OB-Xd 4-pole uses Morph and K35 the continuous saturation,
    // both driven by the extra value, so one configuration each.
    using sfpp::DriveMode;
    using sfpp::FilterModel;
    using sfpp::FilterSubModel;
    using sfpp::Passband;
    using sfpp::Slope;
    struct Expected
    {
        FilterModel model;
        sfpp::ModelConfig config;
    };
    const std::array<Expected, 17> expected { {
        { FilterModel::CytomicSVF, { Passband::LP } },
        { FilterModel::CytomicSVF, { Passband::HP } },
        { FilterModel::CytomicSVF, { Passband::BP } },
        { FilterModel::CytomicSVF, { Passband::Notch } },
        { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::RungeKutta } },
        { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::RungeKuttaCompensated } },
        { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::Huov } },
        { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::HuovCompensated } },
        { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::Huov2010 } },
        { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::HuovCompensated2010 } },
        { FilterModel::OBXD_4Pole, { Passband::LP, Slope::Slope_Morph } },
        { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_6dB } },
        { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_12dB } },
        { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_18dB } },
        { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_24dB } },
        { FilterModel::K35, { Passband::LP, DriveMode::K35_Continuous } },
        { FilterModel::K35, { Passband::HP, DriveMode::K35_Continuous } },
    } };
    const auto variants = filterVariants();
    REQUIRE (variants.size() == expected.size());
    for (size_t i = 0; i < variants.size(); ++i)
    {
        INFO (i << " " << variants[i].name);
        CHECK (variants[i].model == expected[i].model);
        CHECK (variants[i].config.pt == expected[i].config.pt);
        CHECK (variants[i].config.st == expected[i].config.st);
        CHECK (variants[i].config.dt == expected[i].config.dt);
        CHECK (variants[i].config.mt == expected[i].config.mt);
        MultimodeFilter filter;
        CHECK (filter.configure (variants[i]));
    }
}

TEST_CASE ("Each Model entry picks its filter variant", "[dsp][filter]")
{
    // The Cytomic SVF entries map one to one; the other models depend on their setting (ADR 0008)
    for (int model = 0; model < 4; ++model)
        CHECK (chooseFilter (model, defaultSlope, defaultVintage, defaultSaturation).variant == static_cast<size_t> (model));
    for (int variant = 0; variant < 6; ++variant)
        CHECK (chooseFilter (vintageLadder, defaultSlope, variant, defaultSaturation).variant == static_cast<size_t> (4 + variant));
    CHECK (chooseFilter (obxd, defaultSlope, defaultVintage, defaultSaturation).variant == 10);
    CHECK (chooseFilter (k35LowPass, defaultSlope, defaultVintage, defaultSaturation).variant == 15);
    CHECK (chooseFilter (k35HighPass, defaultSlope, defaultVintage, defaultSaturation).variant == 16);
}

TEST_CASE ("The Diode Ladder plays the nearest slope step", "[dsp][filter]")
{
    // ADR 0008: nearest of 6, 12, 18, 24 dB; a tie (9, 15, 21 dB) goes to the steeper step
    const std::array<std::pair<float, size_t>, 9> cases { {
        { 6.0f, 11 }, { 8.9f, 11 }, { 9.0f, 12 }, { 9.1f, 12 }, { 15.0f, 13 }, { 20.9f, 13 }, { 21.0f, 14 }, { 24.0f, 14 }, { 30.0f, 14 },
    } };
    for (auto [slope, variant] : cases)
    {
        INFO ("slope " << slope);
        CHECK (chooseFilter (diodeLadder, slope, defaultVintage, defaultSaturation).variant == variant);
    }
    CHECK (chooseFilter (diodeLadder, -5.0f, defaultVintage, defaultSaturation).variant == 11);
}

TEST_CASE ("The extra value carries the OB-Xd Morph and the K35 saturation", "[dsp][filter]")
{
    // OB-Xd Morph: 0 = 6 dB, 1 = 24 dB (sst-filters OBXDFilter.h:134-168), so (slope - 6) / 18.
    // K35 continuous saturation: 0..1 as given (sst-filters scales it by 4).
    CHECK (chooseFilter (obxd, 6.0f, defaultVintage, defaultSaturation).extra == Catch::Approx (0.0f).margin (1e-6));
    CHECK (chooseFilter (obxd, 15.0f, defaultVintage, defaultSaturation).extra == Catch::Approx (0.5f).margin (1e-6));
    CHECK (chooseFilter (obxd, 24.0f, defaultVintage, defaultSaturation).extra == Catch::Approx (1.0f).margin (1e-6));
    CHECK (chooseFilter (k35LowPass, defaultSlope, defaultVintage, 0.7f).extra == Catch::Approx (0.7f).margin (1e-6));
    CHECK (chooseFilter (k35HighPass, defaultSlope, defaultVintage, 0.0f).extra == Catch::Approx (0.0f).margin (1e-6));
    // Models without a continuous setting get 0
    CHECK (chooseFilter (0, 15.0f, defaultVintage, 0.7f).extra == 0.0f);
    CHECK (chooseFilter (diodeLadder, 15.0f, defaultVintage, 0.7f).extra == 0.0f);
}

TEST_CASE ("chooseFilter bounds every input", "[dsp][filter]")
{
    // Audio thread: whatever the snapshot holds, the result indexes the variant list and the
    // extra value stays in 0..1. A non-finite slope or saturation falls back to its default.
    constexpr auto nan = std::numeric_limits<float>::quiet_NaN();
    constexpr auto inf = std::numeric_limits<float>::infinity();
    CHECK (chooseFilter (-1, defaultSlope, defaultVintage, defaultSaturation).variant == 0);
    CHECK (chooseFilter (99, defaultSlope, defaultVintage, defaultSaturation).variant == 16);
    CHECK (chooseFilter (vintageLadder, defaultSlope, -1, defaultSaturation).variant == 4);
    CHECK (chooseFilter (vintageLadder, defaultSlope, 99, defaultSaturation).variant == 9);
    CHECK (chooseFilter (diodeLadder, nan, defaultVintage, defaultSaturation).variant == 14);
    CHECK (chooseFilter (obxd, nan, defaultVintage, defaultSaturation).extra == Catch::Approx (1.0f));
    CHECK (chooseFilter (obxd, inf, defaultVintage, defaultSaturation).extra == Catch::Approx (1.0f));
    CHECK (chooseFilter (obxd, -inf, defaultVintage, defaultSaturation).extra == Catch::Approx (0.0f));
    CHECK (chooseFilter (k35LowPass, defaultSlope, defaultVintage, nan).extra == Catch::Approx (0.25f));
    CHECK (chooseFilter (k35LowPass, defaultSlope, defaultVintage, 3.0f).extra == Catch::Approx (1.0f));
    CHECK (chooseFilter (k35LowPass, defaultSlope, defaultVintage, -1.0f).extra == Catch::Approx (0.0f));
}

TEST_CASE ("The default settings give back the task 0009 sound", "[dsp][filter]")
{
    // ADR 0008: with Slope 24 dB, Vintage Runge-Kutta compensated and K35 saturation 25 %, each
    // Model entry plays its task 0009 configuration (filterSetups). Noise, 1 s, cutoff 1 kHz,
    // resonance 0.5. Measured 0 on every entry, OB-Xd Morph at 1 included; printed by:
    // ./Builds/DspTests "The default settings give back the task 0009 sound" -s | grep "largest difference"
    const auto input = noise (48000, 23u);
    const auto setups = filterSetups();
    const auto variants = filterVariants();
    for (size_t model = 0; model < setups.size(); ++model)
    {
        const auto choice = chooseFilter (static_cast<int> (model), defaultSlope, defaultVintage, defaultSaturation);
        const auto reference = render (setups[model], 0.0f, input);
        const auto output = render (variants[choice.variant], choice.extra, input);
        float largest = 0.0f;
        for (size_t i = 0; i < output.size(); ++i)
            largest = std::max (largest, std::abs (output[i] - reference[i]));
        SUCCEED (setups[model].name << " largest difference " << largest);
        INFO (setups[model].name);
        CHECK (largest < 1e-6f);
    }
}
