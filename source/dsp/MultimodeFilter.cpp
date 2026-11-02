#include "dsp/MultimodeFilter.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace moodz::dsp
{
    namespace
    {
        // sst-filters takes the cutoff as a note offset from A4: f = 440 * 2^(n / 12)
        // (sst-filters CytomicSVFQuadForm.h, `440 * note_to_pitch_ignoring_tuning`)
        float hertzToNote (float hz) noexcept
        {
            return 12.0f * std::log2 (hz / 440.0f);
        }
    }

    bool MultimodeFilter::configure (const FilterSetup& newSetup)
    {
        namespace sfpp = sst::filtersplusplus;

        // Comb models need caller-owned delay memory: not supported by this processor
        if (sfpp::Filter::requiredDelayLinesSizes (newSetup.model, newSetup.config) != 0)
            return false;

        sfpp::Filter probe;
        probe.setSampleRateAndBlockSize (sampleRate, packetSize);
        probe.setFilterModel (newSetup.model);
        probe.setModelConfiguration (newSetup.config);
        if (!probe.prepareInstance())
            return false;

        setup = newSetup;
        prepare (sampleRate, packetSize);
        return true;
    }

    void MultimodeFilter::prepare (double newSampleRate, int /* maxBlockSize */)
    {
        // The filter works in packets of packetSize, whatever the host block size
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        filter.setSampleRateAndBlockSize (sampleRate, packetSize);
        filter.setFilterModel (setup.model);
        filter.setModelConfiguration (setup.config);
        nyquistGuardHz = static_cast<float> (0.45 * sampleRate);
        ready = setup.model != sst::filtersplusplus::FilterModel::None && filter.prepareInstance();
        filter.setStereo();
        if (ready)
        {
            // Warm up: the first coefficient computation fills sst-filters' shared pitch table
            // (TuningProvider::twoToThe, about 1 000 pow calls). Do it here, off the audio thread.
            filter.makeCoefficients (0, hertzToNote (cutoffHz), resonance);
        }
        filter.init();
        // Clears what the warm-up left in the coefficient smoothing, so the first render after
        // prepare matches a render after reset()
        filter.reset();
        packetPosition = 0;
    }

    void MultimodeFilter::reset() noexcept MOODZ_NONBLOCKING
    {
        // A packet already opened with prepareBlock is closed first, so the next one starts clean
        if (ready && packetPosition != 0)
            filter.concludeBlock();
        filter.reset();
        packetPosition = 0;
    }

    void MultimodeFilter::setCutoffHz (float hz) noexcept MOODZ_NONBLOCKING
    {
        if (!std::isfinite (hz))
            return;
        cutoffHz = std::clamp (hz, minCutoffHz, maxCutoffHz);
    }

    void MultimodeFilter::setResonance (float amount) noexcept MOODZ_NONBLOCKING
    {
        if (!std::isfinite (amount))
            return;
        resonance = std::clamp (amount, 0.0f, 1.0f);
    }

    void MultimodeFilter::process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        if (!ready || channels.empty())
            return;

        auto& left = channels[0];
        const bool stereo = channels.size() > 1;

        auto available = left.size();
        if (stereo)
            available = std::min (available, channels[1].size());
        const int count = std::min (numSamples, static_cast<int> (std::min (available, static_cast<size_t> (std::numeric_limits<int>::max()))));

        for (int i = 0; i < count; ++i)
        {
            if (packetPosition == 0)
            {
                // Linked stereo: compute the coefficients once, copy them to the second voice
                // Called on every packet, even with unchanged parameters, on purpose: each call
                // moves sst-filters' target coefficients 20 % of the way to the new ones
                // (tC = 0.8 tC + 0.2 N, FilterCoefficientMaker_Impl.h:865 at 87a171b) and ramps
                // linearly across the packet. That is the smoothing of cutoff and resonance
                // (.claude/rules/dsp.md); skipping calls would freeze them half way. API contract:
                // "must be called every block before prepareBlock" (filters++/api.h:205).
                // Clamped here, where it is used: the rate may have changed since setCutoffHz
                const float hz = std::min (cutoffHz, nyquistGuardHz);
                filter.makeCoefficients (0, hertzToNote (hz), resonance);
                filter.copyCoefficientsFromVoiceToVoice (0, 1);
                filter.prepareBlock();
            }

            const auto index = static_cast<size_t> (i);
            float outLeft = 0.0f;
            float outRight = 0.0f;
            filter.processStereoSample (left[index], stereo ? channels[1][index] : left[index], outLeft, outRight);
            left[index] = outLeft;
            if (stereo)
                channels[1][index] = outRight;

            if (++packetPosition == packetSize)
            {
                filter.concludeBlock();
                packetPosition = 0;
            }
        }
    }
}
