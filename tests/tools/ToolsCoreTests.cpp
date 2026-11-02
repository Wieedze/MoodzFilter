#include "core/ToolsCore.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace moodz::tools;

namespace
{
    float peakOf (const std::vector<float>& samples)
    {
        float peak = 0.0f;
        for (auto s : samples)
            peak = std::max (peak, std::abs (s));
        return peak;
    }

    int zeroCrossings (std::span<const float> samples)
    {
        int count = 0;
        for (size_t i = 1; i < samples.size(); ++i)
            if ((samples[i - 1] < 0.0f) != (samples[i] < 0.0f))
                ++count;
        return count;
    }

    // Welch estimate: mean power per DFT bin over Hann-windowed frames of frameSize samples
    // (naive DFT, test-only). Returns power for bins 0 .. frameSize / 2.
    std::vector<double> welchPower (const std::vector<float>& samples, int frameSize)
    {
        const auto half = static_cast<size_t> (frameSize / 2);
        std::vector<double> power (half + 1, 0.0);
        int frames = 0;
        for (size_t start = 0; start + static_cast<size_t> (frameSize) <= samples.size(); start += static_cast<size_t> (frameSize))
        {
            for (size_t k = 1; k <= half; ++k)
            {
                double re = 0.0, im = 0.0;
                for (int n = 0; n < frameSize; ++n)
                {
                    const double window = 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * n / frameSize);
                    const double x = window * samples[start + static_cast<size_t> (n)];
                    const double angle = 2.0 * std::numbers::pi * static_cast<double> (k) * n / frameSize;
                    re += x * std::cos (angle);
                    im -= x * std::sin (angle);
                }
                power[k] += re * re + im * im;
            }
            ++frames;
        }
        for (auto& p : power)
            p /= std::max (frames, 1);
        return power;
    }
}

TEST_CASE ("Impulse", "[tools][signals]")
{
    const auto signal = impulse (100);
    REQUIRE (signal.size() == 100);
    CHECK (signal[0] == Catch::Approx (0.25f).margin (1e-9));
    for (size_t i = 1; i < signal.size(); ++i)
        REQUIRE (signal[i] == Catch::Approx (0.0f).margin (1e-12));
}

TEST_CASE ("Sine", "[tools][signals]")
{
    // 1 kHz at 48 kHz: 48 samples per period, so sample 12 is the quarter period (phase pi/2)
    const auto signal = sine (1000.0, 480, 48000.0);
    REQUIRE (signal.size() == 480);
    CHECK (signal[0] == Catch::Approx (0.0f).margin (1e-7));
    CHECK (signal[12] == Catch::Approx (0.25f).margin (1e-6));
    CHECK (signal[36] == Catch::Approx (-0.25f).margin (1e-6));
    CHECK (peakOf (signal) == Catch::Approx (0.25f).margin (1e-6));
}

TEST_CASE ("Exponential sweep", "[tools][signals]")
{
    constexpr int length = 49152;
    const auto signal = exponentialSweep (20.0, 20000.0, length, 48000.0);
    REQUIRE (signal.size() == static_cast<size_t> (length));
    CHECK (signal[0] == Catch::Approx (0.0f).margin (1e-7));
    CHECK (peakOf (signal) <= 0.25f + 1e-6f);

    // Mean frequency over the last 256 samples, from the zero crossing count:
    // f = crossings / 2 / (256 / 48000). The sweep rises about 3.7 % across those samples,
    // so the mean sits a little under 20 kHz.
    const std::span<const float> tail { signal.data() + length - 256, 256 };
    const double meanFrequency = zeroCrossings (tail) / 2.0 / (256.0 / 48000.0);
    CHECK (meanFrequency == Catch::Approx (20000.0).epsilon (0.03));

    // Start: the first 2 400 samples (50 ms) hold about one period of a ~20 Hz tone
    const std::span<const float> head { signal.data(), 2400 };
    CHECK (zeroCrossings (head) >= 1);
    CHECK (zeroCrossings (head) <= 3);
}

TEST_CASE ("Pink noise", "[tools][signals]")
{
    constexpr int length = 24576;
    const auto a = pinkNoise (length, 1u);
    const auto b = pinkNoise (length, 1u);
    REQUIRE (a.size() == static_cast<size_t> (length));
    REQUIRE (a == b);
    CHECK (peakOf (a) == Catch::Approx (0.25f).margin (1e-6));
    CHECK (pinkNoise (length, 2u) != a);

    // Pink noise: power spectral density falls 3 dB per octave (1/f)
    constexpr int frameSize = 1024;
    const auto power = welchPower (a, frameSize);
    const double binHz = 48000.0 / frameSize;
    std::vector<double> bandDb;
    for (double centre = 125.0; centre <= 8000.0; centre *= 2.0)
    {
        const auto low = static_cast<size_t> (std::ceil (centre / std::numbers::sqrt2 / binHz));
        const auto high = static_cast<size_t> (std::floor (centre * std::numbers::sqrt2 / binHz));
        double sum = 0.0;
        for (size_t k = low; k <= high; ++k)
            sum += power[k];
        bandDb.push_back (10.0 * std::log10 (sum / static_cast<double> (high - low + 1)));
    }

    // Least squares slope of PSD (dB) against octave index
    const double n = static_cast<double> (bandDb.size());
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t i = 0; i < bandDb.size(); ++i)
    {
        const double x = static_cast<double> (i);
        sx += x;
        sy += bandDb[i];
        sxx += x * x;
        sxy += x * bandDb[i];
    }
    const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    INFO ("PSD slope " << slope << " dB per octave");
    CHECK (slope < -2.0);
    CHECK (slope > -4.0);
}

TEST_CASE ("Difference metrics", "[tools][compare]")
{
    std::vector<float> reference (1000, 0.1f);

    SECTION ("identical buffers")
    {
        const auto difference = measureDifference (reference, reference);
        CHECK (difference.maxAbsError == Catch::Approx (0.0).margin (1e-12));
        CHECK (difference.rmsErrorDbfs <= silenceDbfs);
    }

    // Silent reference so that the differences are exactly the float values 0.001f and 0.01f
    // (float spacing near 0.1 is about 7.5e-9, which would hide a 1e-9 tolerance)
    const std::vector<float> silence (1000, 0.0f);

    SECTION ("0.001 everywhere: max 0.001, RMS 20*log10(0.001) = -60 dBFS")
    {
        const std::vector<float> shifted (1000, 0.001f);
        const auto difference = measureDifference (shifted, silence);
        CHECK (difference.maxAbsError == Catch::Approx (0.001).margin (1e-9));
        CHECK (difference.rmsErrorDbfs == Catch::Approx (-60.0).margin (0.01));
    }

    SECTION ("one sample off by 0.01")
    {
        auto changed = silence;
        changed[500] = 0.01f;
        const auto difference = measureDifference (changed, silence);
        CHECK (difference.maxAbsError == Catch::Approx (0.01).margin (1e-9));
        // RMS = sqrt(0.01^2 / 1000) -> 20*log10 = -40 - 30 = -70 dBFS
        CHECK (difference.rmsErrorDbfs == Catch::Approx (-70.0).margin (0.01));
    }
}
