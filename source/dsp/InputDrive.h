#pragma once

#include "dsp/Realtime.h"

#include <array>
#include <span>

namespace moodz::dsp
{
    // The saturation before the filter, inside the oversampling (task 0013, ADR 0009). Prepared at
    // the rate it runs at. Transparent at 0 dB. Linked stereo: one gain, one DC blocker per channel.
    class InputDrive
    {
    public:
        static constexpr float minDb = 0.0f;
        static constexpr float maxDb = 24.0f;
        static constexpr double rampSeconds = 0.02;
        static constexpr std::array curveLabels { "Symmetric", "Asymmetric" };
        static constexpr int defaultCurveIndex = 0;

        void prepare (double sampleRate, int maxBlockSize) noexcept;
        // Jumps to the targets: no ramp after reset(), the DC blockers cleared.
        void reset() noexcept MOODZ_NONBLOCKING;

        // Clamped; a non-finite value is ignored.
        void setDriveDb (float db) noexcept MOODZ_NONBLOCKING;
        // Bounded to the curve list.
        void setCurveIndex (int index) noexcept MOODZ_NONBLOCKING;

        // One or two channels; every span holds at least numSamples samples.
        void process (std::span<const std::span<float>> channels, int numSamples) noexcept MOODZ_NONBLOCKING;

    private:
        // A linear ramp over the given length; a new target restarts it from the current value
        struct Ramp
        {
            float value = 0.0f;
            float start = 0.0f;
            float target = 0.0f;
            float step = 0.0f;
            int left = 0;

            void setTarget (float newTarget, int length) noexcept MOODZ_NONBLOCKING;
            void jump() noexcept MOODZ_NONBLOCKING;
            void advance (int length) noexcept MOODZ_NONBLOCKING;
        };

        struct DcBlocker
        {
            float previousInput = 0.0f;
            float previousOutput = 0.0f;
        };

        void clearDcBlockers() noexcept MOODZ_NONBLOCKING;

        int rampLength = 960;
        float dcCoefficient = 0.0f;
        float targetDb = minDb;
        Ramp gain { 1.0f, 1.0f, 1.0f };
        Ramp wet; // 0 is all dry, 1 all saturated
        Ramp curve; // 0 is Symmetric, 1 Asymmetric
        std::array<DcBlocker, 2> dcBlockers {};
    };
}
