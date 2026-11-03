#pragma once

#include "dsp/MultimodeFilter.h"

#include <array>

namespace moodz::dsp
{
    // The filter list of v1, in the order of the Model parameter (playbook ADRs 0006, 0007): the
    // 5 models chosen by ear in task 0008, each with every passband it has, so no entry plays
    // something other than its name. The order is frozen: entries can only be added at the end.
    // Passbands and configurations taken from Filter::availableModelConfigurations at
    // sst-filters 87a171b; the per-model settings keep their task 0008 audition values until
    // task 0010.
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
}
