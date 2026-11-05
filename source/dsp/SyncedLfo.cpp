#include "dsp/SyncedLfo.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace moodz::dsp
{
    void SyncedLfo::prepare (double newSampleRate) noexcept
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
        if (!std::isfinite (quarterNotes))
            return;
        const double cycles = quarterNotes / rates[static_cast<std::size_t> (rate)].quarterNotes;
        currentPhase = cycles - std::floor (cycles);
    }

    void SyncedLfo::advance (int samples) noexcept MOODZ_NONBLOCKING
    {
        // Quarter notes per sample = BPM / (60 x rate)
        const double step = tempoBpm / (60.0 * sampleRate * rates[static_cast<std::size_t> (rate)].quarterNotes);
        const double next = currentPhase + static_cast<double> (samples) * step;
        currentPhase = next - std::floor (next);
    }

    double SyncedLfo::phase() const noexcept
    {
        return currentPhase;
    }

    float SyncedLfo::value() const noexcept MOODZ_NONBLOCKING
    {
        // Definitions in docs/tasks/0015-synced-lfo.md. A hair of phase ahead: an edge that falls exactly
        // on a step of the modulation grid must not land on either side by the rounding of the phase,
        // which depends on the block size
        const double ahead = currentPhase + 1e-9;
        const double p = ahead - std::floor (ahead);
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
