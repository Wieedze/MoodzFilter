#pragma once

#include "dsp/MultimodeFilter.h"

#include <algorithm>
#include <array>

namespace moodz::dsp
{
    // The five filter models of v1, chosen by ear in task 0008 (playbook ADR 0006). The order is
    // frozen: it is the order of the Model parameter, and models can only be added at the end.
    inline constexpr std::array<const char*, 5> filterModelLabels { "Cytomic SVF", "Vintage Ladder", "OB-Xd 4-pole", "Diode Ladder", "K35" };
    inline constexpr std::size_t filterModelCount = filterModelLabels.size();

    // SPEC Mode, in the order of the Mode parameter
    inline constexpr std::array<const char*, 4> filterModeLabels { "Low-pass", "High-pass", "Band-pass", "Notch" };
    inline constexpr std::size_t filterModeCount = filterModeLabels.size();

    // Every (model, passband) pair the models support, in model order then mode order. Passbands
    // and configurations taken from Filter::availableModelConfigurations at sst-filters 87a171b;
    // the per-model settings keep their task 0008 audition values until task 0010.
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

    // The filter setup that plays a model in a mode. A mode the model lacks plays its low pass
    // (ADR 0006). Indices out of range are clamped, so the result is always a valid setup.
    constexpr std::size_t filterSetupIndex (int model, int mode) noexcept
    {
        constexpr std::array<std::array<std::size_t, filterModeCount>, filterModelCount> table { {
            { 0, 1, 2, 3 }, // Cytomic SVF: low, high, band, notch
            { 4, 4, 4, 4 }, // Vintage Ladder: low pass only
            { 5, 5, 5, 5 }, // OB-Xd 4-pole: low pass only
            { 6, 6, 6, 6 }, // Diode Ladder: low pass only
            { 7, 8, 7, 7 }, // K35: low and high pass
        } };
        const auto m = static_cast<std::size_t> (std::clamp (model, 0, static_cast<int> (filterModelCount) - 1));
        const auto p = static_cast<std::size_t> (std::clamp (mode, 0, static_cast<int> (filterModeCount) - 1));
        return table[m][p];
    }
}
