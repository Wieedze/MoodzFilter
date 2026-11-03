#include "dsp/FilterModels.h"

#include <algorithm>
#include <cmath>

namespace moodz::dsp
{
    namespace
    {
        // Positions of the Model entries (filterSetups) and of their first variant (filterVariants)
        enum ModelEntry : int
        {
            vintageLadderEntry = 4,
            obxdEntry = 5,
            diodeLadderEntry = 6,
            k35LowPassEntry = 7,
            k35HighPassEntry = 8,
        };
        constexpr std::size_t firstVintageVariant = 4, obxdVariant = 10, firstDiodeVariant = 11, k35LowPassVariant = 15, k35HighPassVariant = 16;
        constexpr int diodeSteps = 4; // 6, 12, 18, 24 dB
        constexpr float defaultSaturation = 0.25f; // sst-filters K35 Mild (ADR 0008)
    }

    FilterChoice chooseFilter (int modelIndex, float slopeDb, int vintageVariant, float k35Saturation) noexcept MOODZ_NONBLOCKING
    {
        const int model = std::clamp (modelIndex, 0, static_cast<int> (filterSetupCount) - 1);
        // Bounded in float before any conversion to int (NaN and infinity are undefined there)
        const float slope = std::isnan (slopeDb) ? maxSlopeDb : std::clamp (slopeDb, minSlopeDb, maxSlopeDb);
        const float saturation = std::isfinite (k35Saturation) ? std::clamp (k35Saturation, 0.0f, 1.0f) : defaultSaturation;
        // Position of the slope between 6 dB (0) and 24 dB (1)
        const float slopePosition = (slope - minSlopeDb) / (maxSlopeDb - minSlopeDb);

        switch (model)
        {
            case vintageLadderEntry:
                return { firstVintageVariant + static_cast<std::size_t> (std::clamp (vintageVariant, 0, static_cast<int> (vintageVariantLabels.size()) - 1)), 0.0f };
            case obxdEntry:
                // OB-Xd Morph: 0 = 6 dB, 1/3 = 12, 2/3 = 18, 1 = 24 dB (sst-filters OBXDFilter.h:134-168)
                return { obxdVariant, slopePosition };
            case diodeLadderEntry:
            {
                // Nearest 6 dB step; a tie rounds up, to the steeper step (ADR 0008)
                const auto step = std::min (static_cast<int> (std::floor (slopePosition * (diodeSteps - 1) + 0.5f)), diodeSteps - 1);
                return { firstDiodeVariant + static_cast<std::size_t> (step), 0.0f };
            }
            case k35LowPassEntry:
                return { k35LowPassVariant, saturation };
            case k35HighPassEntry:
                return { k35HighPassVariant, saturation };
            default:
                // Cytomic SVF entries: one variant each, in the same position
                return { static_cast<std::size_t> (model), 0.0f };
        }
    }
}
