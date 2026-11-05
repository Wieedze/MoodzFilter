#pragma once

#include "dsp/Realtime.h"

#include <array>

namespace moodz::dsp
{
    // A phase in time with the host tempo, and four shapes (task 0015). No smoothing here: the
    // oversampled filter chain reads it on its modulation grid.
    class SyncedLfo
    {
    public:
        static constexpr std::array shapeLabels { "Sine", "Triangle", "Sawtooth", "Square" };
        static constexpr int defaultShapeIndex = 0;

        struct Rate
        {
            const char* label;
            double quarterNotes;
        };
        // Longest to shortest; a bar is 4 quarter notes. Frozen order: new values at the end only.
        static constexpr std::array rates {
            Rate { "4 bars", 16.0 },
            Rate { "2 bars D", 12.0 },
            Rate { "4 bars T", 32.0 / 3.0 },
            Rate { "2 bars", 8.0 },
            Rate { "1 bar D", 6.0 },
            Rate { "2 bars T", 16.0 / 3.0 },
            Rate { "1 bar", 4.0 },
            Rate { "1/2 D", 3.0 },
            Rate { "1 bar T", 8.0 / 3.0 },
            Rate { "1/2", 2.0 },
            Rate { "1/4 D", 1.5 },
            Rate { "1/2 T", 4.0 / 3.0 },
            Rate { "1/4", 1.0 },
            Rate { "1/8 D", 0.75 },
            Rate { "1/4 T", 2.0 / 3.0 },
            Rate { "1/8", 0.5 },
            Rate { "1/16 D", 0.375 },
            Rate { "1/8 T", 1.0 / 3.0 },
            Rate { "1/16", 0.25 },
            Rate { "1/32 D", 0.1875 },
            Rate { "1/16 T", 1.0 / 6.0 },
            Rate { "1/32", 0.125 }
        };
        static constexpr int defaultRateIndex = 12; // 1/4
        static constexpr double defaultTempoBpm = 120.0;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept MOODZ_NONBLOCKING;

        // Bounded to their lists.
        void setShapeIndex (int index) noexcept MOODZ_NONBLOCKING;
        void setRateIndex (int index) noexcept MOODZ_NONBLOCKING;
        // A non-finite or non-positive tempo is ignored: the last valid one stays.
        void setTempoBpm (double bpm) noexcept MOODZ_NONBLOCKING;
        // Sets the phase from the song position in quarter notes; a non-finite one is ignored.
        void syncToPosition (double quarterNotes) noexcept MOODZ_NONBLOCKING;

        void advance (int samples) noexcept MOODZ_NONBLOCKING;
        double phase() const noexcept;
        // -1 to 1
        float value() const noexcept MOODZ_NONBLOCKING;

    private:
        double sampleRate = 48000.0;
        double tempoBpm = defaultTempoBpm;
        double currentPhase = 0.0; // 0 to 1
        int shape = defaultShapeIndex;
        int rate = defaultRateIndex;
    };
}
