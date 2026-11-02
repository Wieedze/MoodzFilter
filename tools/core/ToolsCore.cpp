#include "core/ToolsCore.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

namespace moodz::tools
{
    std::vector<float> impulse (int length)
    {
        std::vector<float> samples (static_cast<size_t> (std::max (length, 0)), 0.0f);
        if (!samples.empty())
            samples[0] = signalPeak;
        return samples;
    }

    std::vector<float> sine (double frequency, int length, double sampleRate)
    {
        std::vector<float> samples (static_cast<size_t> (std::max (length, 0)));
        for (size_t n = 0; n < samples.size(); ++n)
            samples[n] = signalPeak * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * static_cast<double> (n) / sampleRate));
        return samples;
    }

    std::vector<float> exponentialSweep (double startFrequency, double endFrequency, int length, double sampleRate)
    {
        // A. Farina, "Simultaneous measurement of impulse response and distortion with a
        // swept-sine technique", AES 108th Convention, 2000, eq. 2:
        // x(t) = sin (2 pi f1 T / ln(f2/f1) * (exp (t / T * ln(f2/f1)) - 1))
        std::vector<float> samples (static_cast<size_t> (std::max (length, 0)));
        const double duration = static_cast<double> (length) / sampleRate;
        const double logRatio = std::log (endFrequency / startFrequency);
        for (size_t n = 0; n < samples.size(); ++n)
        {
            const double t = static_cast<double> (n) / sampleRate;
            const double phase = 2.0 * std::numbers::pi * startFrequency * duration / logRatio * (std::exp (t / duration * logRatio) - 1.0);
            samples[n] = signalPeak * static_cast<float> (std::sin (phase));
        }
        return samples;
    }

    std::vector<float> pinkNoise (int length, std::uint32_t seed)
    {
        std::vector<float> samples (static_cast<size_t> (std::max (length, 0)));

        // White noise from a 32-bit LCG (Numerical Recipes constants): deterministic, no rand()
        std::uint32_t state = seed;
        // Pink filter: Paul Kellet's "economy" version, three one-pole filters, +-0.5 dB
        // above 9.2 Hz at 44.1 kHz (music-dsp mailing list, 1999)
        double b0 = 0.0, b1 = 0.0, b2 = 0.0;
        for (auto& sample : samples)
        {
            state = state * 1664525u + 1013904223u;
            const double white = static_cast<double> (state) / 4294967296.0 * 2.0 - 1.0;
            b0 = 0.99765 * b0 + white * 0.0990460;
            b1 = 0.96300 * b1 + white * 0.2965164;
            b2 = 0.57000 * b2 + white * 1.0526913;
            sample = static_cast<float> (b0 + b1 + b2 + white * 0.1848);
        }

        float peak = 0.0f;
        for (auto s : samples)
            peak = std::max (peak, std::abs (s));
        if (peak > 0.0f)
            for (auto& s : samples)
                s *= signalPeak / peak;
        return samples;
    }

    Difference measureDifference (std::span<const float> actual, std::span<const float> expected)
    {
        assert (actual.size() == expected.size());
        Difference difference;
        double sumOfSquares = 0.0;
        for (size_t i = 0; i < actual.size(); ++i)
        {
            const double error = static_cast<double> (actual[i]) - static_cast<double> (expected[i]);
            difference.maxAbsError = std::max (difference.maxAbsError, std::abs (error));
            sumOfSquares += error * error;
        }

        const double rms = actual.empty() ? 0.0 : std::sqrt (sumOfSquares / static_cast<double> (actual.size()));
        difference.rmsErrorDbfs = rms > 0.0 ? std::max (20.0 * std::log10 (rms), silenceDbfs) : silenceDbfs;
        return difference;
    }
}
