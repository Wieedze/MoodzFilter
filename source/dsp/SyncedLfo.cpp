#include "dsp/SyncedLfo.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace moodz::dsp
{
    namespace
    {
        // The fractional part, in [0, 1): for a tiny negative value, x - floor (x) rounds to 1
        double wrapped (double x) noexcept
        {
            const double fraction = x - std::floor (x);
            return fraction < 1.0 ? fraction : 0.0;
        }
    }

    void SyncedLfo::prepare (double newSampleRate, int /* maxBlockSize */) noexcept
    {
        sampleRate = std::isfinite (newSampleRate) && newSampleRate > 0.0 ? newSampleRate : 48000.0;
        reset();
    }

    void SyncedLfo::reset() noexcept MOODZ_NONBLOCKING
    {
        currentPhase = 0.0;
    }

    void SyncedLfo::setShapeIndex (int index) noexcept MOODZ_NONBLOCKING
    {
        shape = std::clamp (index, 0, static_cast<int> (shapeLabels.size()) - 1);
    }

    void SyncedLfo::setRateIndex (int index) noexcept MOODZ_NONBLOCKING
    {
        rate = std::clamp (index, 0, static_cast<int> (rates.size()) - 1);
    }

    void SyncedLfo::setTempoBpm (double bpm) noexcept MOODZ_NONBLOCKING
    {
        if (std::isfinite (bpm) && bpm > 0.0)
            tempoBpm = bpm;
    }

    void SyncedLfo::syncToPosition (double quarterNotes) noexcept MOODZ_NONBLOCKING
    {
        // Finite after the division too: on a short rate an extreme position overflows
        const double cycles = quarterNotes / rates[static_cast<std::size_t> (rate)].quarterNotes;
        if (std::isfinite (cycles))
            currentPhase = wrapped (cycles);
    }

    void SyncedLfo::advance (int samples) noexcept MOODZ_NONBLOCKING
    {
        // Cycles per sample = BPM / (60 x sample rate x quarter notes per cycle)
        const double step = tempoBpm / (60.0 * sampleRate * rates[static_cast<std::size_t> (rate)].quarterNotes);
        const double next = currentPhase + static_cast<double> (samples) * step;
        if (std::isfinite (next))
            currentPhase = wrapped (next);
    }

    double SyncedLfo::phase() const noexcept
    {
        return currentPhase;
    }

    float SyncedLfo::value() const noexcept MOODZ_NONBLOCKING
    {
        // Definitions in docs/tasks/0015-synced-lfo.md. A hair of phase ahead: an edge that falls exactly
        // on a step of the modulation grid must not land on either side by the rounding of the phase,
        // which depends on the block size. A host position rounded more coarsely can still move an
        // edge by one step of the grid, under 1 ms at 48 kHz.
        const double p = wrapped (currentPhase + 1e-9);
        switch (shape)
        {
            case 1:
                return static_cast<float> (p < 0.25 ? 4.0 * p : (p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0));
            case 2:
                return static_cast<float> (2.0 * p - 1.0);
            case 3:
                return p < 0.5 ? 1.0f : -1.0f;
            default:
                return static_cast<float> (std::sin (2.0 * std::numbers::pi * p));
        }
    }
}
