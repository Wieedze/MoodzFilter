#include "dsp/InputDrive.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace moodz::dsp
{
    namespace
    {
        // docs/tasks/0013-input-drive.md, decision notes
        constexpr float wetFullDb = 6.0f;
        constexpr double dcCutoffHz = 10.0;
        // tanh (0.3): python3 -c "from math import tanh; print(tanh(.3))"
        constexpr float biasTanh = 0.29131261f;

        // (tanh (u + b) - tanh (b)) / (1 - tanh^2 (b)), written with the tanh addition formula as
        // t / (1 + t tanh (b)), t = tanh (u): exactly 0 at 0, and the denominator stays above 0
        float asymmetricCurve (float t) noexcept MOODZ_NONBLOCKING
        {
            return t / (1.0f + t * biasTanh);
        }
    }

    void InputDrive::Ramp::setTarget (float newTarget, int length) noexcept MOODZ_NONBLOCKING
    {
        target = newTarget;
        start = value;
        step = (target - value) / static_cast<float> (length);
        left = length;
    }

    void InputDrive::Ramp::jump() noexcept MOODZ_NONBLOCKING
    {
        value = start = target;
        step = 0.0f;
        left = 0;
    }

    void InputDrive::Ramp::advance (int length) noexcept MOODZ_NONBLOCKING
    {
        if (left > 0)
        {
            --left;
            // From the start of the ramp, so float rounding does not drift; the last sample lands on the target
            value = left == 0 ? target : start + step * static_cast<float> (length - left);
        }
    }

    void InputDrive::prepare (double sampleRate, int /* maxBlockSize */) noexcept
    {
        const double rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        rampLength = std::max (1, static_cast<int> (std::lround (rampSeconds * rate)));
        // R = exp (-2 pi fc / fs): J. O. Smith, Introduction to Digital Filters, "DC Blocker"
        dcCoefficient = static_cast<float> (std::exp (-2.0 * std::numbers::pi * dcCutoffHz / rate));
        reset();
    }

    void InputDrive::clearDcBlockers() noexcept MOODZ_NONBLOCKING
    {
        dcBlockers = {};
    }

    void InputDrive::reset() noexcept MOODZ_NONBLOCKING
    {
        gain.jump();
        wet.jump();
        curve.jump();
        clearDcBlockers();
    }

    void InputDrive::setDriveDb (float db) noexcept MOODZ_NONBLOCKING
    {
        if (!std::isfinite (db))
            return;
        // Called every block: recompute only when the target really changes
        const float clamped = std::clamp (db, minDb, maxDb);
        if (clamped == targetDb)
            return;
        targetDb = clamped;
        // Decibels to linear amplitude: g = 10^(dB / 20)
        gain.setTarget (std::pow (10.0f, clamped / 20.0f), rampLength);
        wet.setTarget (std::min (clamped / wetFullDb, 1.0f), rampLength);
    }

    void InputDrive::setCurveIndex (int index) noexcept MOODZ_NONBLOCKING
    {
        const float newTarget = index >= 1 ? 1.0f : 0.0f;
        if (newTarget != curve.target)
            curve.setTarget (newTarget, rampLength);
    }

    void InputDrive::process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING
    {
        const auto numChannels = std::min (channels.size(), dcBlockers.size());
        for (int i = 0; i < numSamples; ++i)
        {
            gain.advance (rampLength);
            wet.advance (rampLength);
            curve.advance (rampLength);
            // At rest at 0 dB: the signal is left as it is, and a curve change has nothing to blend
            if (wet.value <= 0.0f && wet.left == 0)
            {
                curve.jump();
                clearDcBlockers();
                return;
            }

            const bool symmetricHeard = curve.value < 1.0f;
            const bool asymmetricHeard = curve.value > 0.0f || curve.left > 0;
            if (!asymmetricHeard)
                clearDcBlockers();
            const auto index = static_cast<std::size_t> (i);
            for (std::size_t c = 0; c < numChannels; ++c)
            {
                const float x = channels[c][index];
                const float t = std::tanh (gain.value * x);
                float saturated = 0.0f;
                if (symmetricHeard)
                    saturated += (1.0f - curve.value) * (t - x);
                if (asymmetricHeard)
                {
                    // The DC blocker works on the difference only, so the dry signal is never filtered;
                    // y[n] = x[n] - x[n-1] + R y[n-1]: J. O. Smith, Introduction to Digital Filters, "DC Blocker"
                    float difference = asymmetricCurve (t) - x;
                    if (!std::isfinite (difference))
                        difference = 0.0f;
                    auto& blocker = dcBlockers[c];
                    const float blocked = difference - blocker.previousInput + dcCoefficient * blocker.previousOutput;
                    blocker.previousInput = difference;
                    blocker.previousOutput = blocked;
                    saturated += curve.value * blocked;
                }
                channels[c][index] = x + wet.value * saturated;
            }
        }
    }
}
