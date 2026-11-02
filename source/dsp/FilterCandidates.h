#pragma once

#include "dsp/MultimodeFilter.h"

#include <array>

namespace moodz::dsp
{
    // The sst-filters models auditioned to pick the 4 models of v1 (SPEC: a clean multimode,
    // a ladder, an OB-X style model, a diode). Low pass for now; task 0008 chooses by ear.
    // Configurations taken from Filter::availableModelConfigurations at sst-filters 87a171b.
    inline std::array<FilterSetup, 7> filterCandidates()
    {
        namespace sfpp = sst::filtersplusplus;
        using sfpp::DriveMode;
        using sfpp::FilterModel;
        using sfpp::FilterSubModel;
        using sfpp::Passband;
        using sfpp::Slope;

        return { {
            { FilterModel::CytomicSVF, { Passband::LP }, "cytomic-svf" },
            { FilterModel::VemberClassic, { Passband::LP, Slope::Slope_24dB, DriveMode::Standard }, "vember-classic-24" },
            { FilterModel::VintageLadder, { Passband::LP, FilterSubModel::RungeKuttaCompensated }, "vintage-ladder" },
            { FilterModel::OBXD_4Pole, { Passband::LP, Slope::Slope_24dB }, "obxd-4pole-24" },
            { FilterModel::OBXD_2Pole, { Passband::LP, DriveMode::Standard }, "obxd-2pole" },
            { FilterModel::DiodeLadder, { Passband::LP, Slope::Slope_24dB }, "diode-ladder-24" },
            { FilterModel::K35, { Passband::LP, DriveMode::K35_Mild }, "k35-mild" },
        } };
    }
}
