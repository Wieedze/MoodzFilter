#pragma once

#include "dsp/MultimodeFilter.h"
#include "dsp/Realtime.h"

#include <array>
#include <cstddef>

namespace moodz::dsp
{
    // The filter list of v1, in the order of the Model parameter (playbook ADRs 0006, 0007): the
    // 5 models chosen by ear in task 0008, each with every passband it has, so no entry plays
    // something other than its name. The order is frozen: entries can only be added at the end.
    // Passbands and configurations taken from Filter::availableModelConfigurations at
    // sst-filters 87a171b. Each entry holds its task 0008 audition configuration, which the
    // default per-model settings give back (ADR 0008); the processor plays filterVariants().
    inline constexpr std::size_t filterSetupCount = 9;

    inline std::array<FilterSetup, filterSetupCount> filterSetups()
    {
        namespace sfpp = sst::filtersplusplus;
        using sfpp::DriveMode;
        using sfpp::FilterModel;
        using sfpp::FilterSubModel;
        using sfpp::Passband;
        using sfpp::Slope;

        return { {
            { FilterModel::CytomicSVF, { Passband::LP }, "cytomic-svf-lp", "Cytomic SVF Low-pass" },
            { FilterModel::CytomicSVF, { Passband::HP }, "cytomic-svf-hp", "Cytomic SVF High-pass" },
            { FilterModel::CytomicSVF, { Passband::BP }, "cytomic-svf-bp", "Cytomic SVF Band-pass" },
            { FilterModel::CytomicSVF, { Passband::Notch }, "cytomic-svf-notch", "Cytomic SVF Notch" },
            { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::RungeKuttaCompensated }, "vintage-ladder-lp", "Vintage Ladder Low-pass" },
            { FilterModel::OBXD_4Pole, { Passband::LP, Slope::Slope_24dB }, "obxd-4pole-lp", "OB-Xd 4-pole Low-pass" },
            { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_24dB }, "diode-ladder-lp", "Diode Ladder Low-pass" },
            { FilterModel::K35, { Passband::LP, DriveMode::K35_Mild }, "k35-lp", "K35 Low-pass" },
            { FilterModel::K35, { Passband::HP, DriveMode::K35_Mild }, "k35-hp", "K35 High-pass" },
        } };
    }

    // Every configuration the parameters reach (ADR 0008), each prepared once off the audio
    // thread: the Model entries, with one configuration per Vintage Ladder sub-model and per
    // Diode Ladder slope step. OB-Xd 4-pole (Morph) and K35 (continuous saturation) take their
    // setting as the extra value, so they need one configuration each.
    inline constexpr std::size_t filterVariantCount = 17;

    inline std::array<FilterSetup, filterVariantCount> filterVariants()
    {
        namespace sfpp = sst::filtersplusplus;
        using sfpp::DriveMode;
        using sfpp::FilterModel;
        using sfpp::FilterSubModel;
        using sfpp::Passband;
        using sfpp::Slope;

        return { {
            { FilterModel::CytomicSVF, { Passband::LP }, "cytomic-svf-lp", "Cytomic SVF Low-pass" },
            { FilterModel::CytomicSVF, { Passband::HP }, "cytomic-svf-hp", "Cytomic SVF High-pass" },
            { FilterModel::CytomicSVF, { Passband::BP }, "cytomic-svf-bp", "Cytomic SVF Band-pass" },
            { FilterModel::CytomicSVF, { Passband::Notch }, "cytomic-svf-notch", "Cytomic SVF Notch" },
            { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::RungeKutta }, "vintage-ladder-rk", "Vintage Ladder Runge-Kutta" },
            { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::RungeKuttaCompensated }, "vintage-ladder-rk-compensated", "Vintage Ladder Runge-Kutta compensated" },
            { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::Huov }, "vintage-ladder-huov", "Vintage Ladder Huovilainen" },
            { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::HuovCompensated }, "vintage-ladder-huov-compensated", "Vintage Ladder Huovilainen compensated" },
            { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::Huov2010 }, "vintage-ladder-huov2010", "Vintage Ladder Huovilainen 2010" },
            { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::HuovCompensated2010 }, "vintage-ladder-huov2010-compensated", "Vintage Ladder Huovilainen 2010 compensated" },
            { FilterModel::OBXD_4Pole, { Passband::LP, Slope::Slope_Morph }, "obxd-4pole-morph", "OB-Xd 4-pole Morph" },
            { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_6dB }, "diode-ladder-6db", "Diode Ladder 6 dB" },
            { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_12dB }, "diode-ladder-12db", "Diode Ladder 12 dB" },
            { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_18dB }, "diode-ladder-18db", "Diode Ladder 18 dB" },
            { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_24dB }, "diode-ladder-24db", "Diode Ladder 24 dB" },
            { FilterModel::K35, { Passband::LP, DriveMode::K35_Continuous }, "k35-lp", "K35 Low-pass" },
            { FilterModel::K35, { Passband::HP, DriveMode::K35_Continuous }, "k35-hp", "K35 High-pass" },
        } };
    }

    // The Vintage variant choices, in the order of the variants above (sst-filters FilterSubModel)
    inline constexpr std::array<const char*, 6> vintageVariantLabels {
        "Runge-Kutta",
        "Runge-Kutta compensated",
        "Huovilainen",
        "Huovilainen compensated",
        "Huovilainen 2010",
        "Huovilainen 2010 compensated",
    };

    inline constexpr float minSlopeDb = 6.0f;
    inline constexpr float maxSlopeDb = 24.0f;

    struct FilterChoice
    {
        std::size_t variant = 0; // index in filterVariants()
        float extra = 0.0f; // 0..1, MultimodeFilter::setExtra
    };

    // The filter variant and extra value for a Model entry and the per-model settings (ADR 0008).
    // slopeDb in [minSlopeDb, maxSlopeDb], k35Saturation in 0..1; every input is bounded, and a
    // non-finite slope or saturation falls back to its default (24 dB, 0.25).
    FilterChoice chooseFilter (int modelIndex, float slopeDb, int vintageVariant, float k35Saturation) noexcept MOODZ_NONBLOCKING;
}
