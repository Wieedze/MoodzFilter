#include "dsp/InputDrive.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

using Catch::Approx;
using moodz::dsp::InputDrive;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int rampLength = 960; // 20 ms at 48 kHz, as Gain out
    constexpr int symmetric = 0, asymmetric = 1;

    void prepared (InputDrive& drive, float db, int curve)
    {
        drive.setDriveDb (db);
        drive.setCurveIndex (curve);
        drive.prepare (sampleRate, 512);
        drive.reset();
    }

    void run (InputDrive& drive, std::span<float> samples)
    {
        const std::array<std::span<float>, 1> channels { samples };
        drive.process (channels, static_cast<int> (samples.size()));
    }

    // The first sample after a reset: the DC blocker has no history, so it passes it whole
    float firstSample (float db, int curve, float x)
    {
        InputDrive drive;
        prepared (drive, db, curve);
        std::array<float, 1> sample { x };
        run (drive, sample);
        return sample[0];
    }

    std::vector<float> sine (double frequency, float peak, int length)
    {
        std::vector<float> samples (static_cast<size_t> (length));
        for (int i = 0; i < length; ++i)
            samples[static_cast<size_t> (i)] = peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * i / sampleRate));
        return samples;
    }

    // Deterministic noise (32-bit LCG, Numerical Recipes constants), peak 0.25
    std::vector<float> noise (int length, std::uint32_t seed)
    {
        std::vector<float> samples (static_cast<size_t> (length));
        for (auto& s : samples)
        {
            seed = seed * 1664525u + 1013904223u;
            s = 0.25f * (static_cast<float> (seed) / 4294967296.0f * 2.0f - 1.0f);
        }
        return samples;
    }

    // Amplitude at one frequency, over a whole number of its periods (one point of a DFT)
    double amplitude (std::span<const float> samples, double frequency)
    {
        double re = 0.0, im = 0.0;
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const double phase = 2.0 * std::numbers::pi * frequency * static_cast<double> (i) / sampleRate;
            re += static_cast<double> (samples[i]) * std::cos (phase);
            im += static_cast<double> (samples[i]) * std::sin (phase);
        }
        return 2.0 * std::hypot (re, im) / static_cast<double> (samples.size());
    }

    double rms (std::span<const float> samples)
    {
        double sum = 0.0;
        for (auto s : samples)
            sum += static_cast<double> (s) * s;
        return std::sqrt (sum / static_cast<double> (samples.size()));
    }
}

TEST_CASE ("Drive at 0 dB leaves the signal untouched", "[dsp][drive]")
{
    // ADR 0009, both curves, noise
    const auto input = noise (4096, 71u);
    for (int curve : { symmetric, asymmetric })
    {
        INFO ("curve " << curve);
        InputDrive drive;
        prepared (drive, 0.0f, curve);
        auto output = input;
        run (drive, output);
        for (std::size_t i = 0; i < output.size(); ++i)
            REQUIRE (std::abs (output[i] - input[i]) < 1e-7f);
    }
}

TEST_CASE ("The Drive curves follow their definitions", "[dsp][drive]")
{
    // Values from the curve definitions in docs/tasks/0013-input-drive.md:
    // python3 -c "from math import tanh; g=10**(12/20); t=tanh(.3); a=lambda u:(tanh(u+.3)-t)/(1-t*t); print(tanh(g*.5),a(g*.5),a(-g*.5),g*.001,a(g*.001),.5+.5*(tanh(10**(3/20)*.5)-.5))"
    CHECK (firstSample (12.0f, symmetric, 0.5f) == Approx (0.96335).margin (1e-4));
    CHECK (firstSample (12.0f, symmetric, -0.5f) == Approx (-0.96335).margin (1e-4));
    CHECK (firstSample (12.0f, asymmetric, 0.5f) == Approx (0.75225).margin (1e-3));
    CHECK (firstSample (12.0f, asymmetric, -0.5f) == Approx (-1.33917).margin (1e-3));
    // The same small signal gain, the drive gain
    CHECK (firstSample (12.0f, symmetric, 0.001f) == Approx (0.00398).margin (1e-5));
    CHECK (firstSample (12.0f, asymmetric, 0.001f) == Approx (0.00398).margin (1e-5));
    // Half dry, half saturated at 3 dB
    CHECK (firstSample (3.0f, symmetric, 0.5f) == Approx (0.55417).margin (1e-3));
}

TEST_CASE ("Drive has no hole in its travel", "[dsp][drive]")
{
    // 1 kHz sine at -12 dBFS, Drive 0 to 24 dB in steps of 0.5 dB: the RMS never falls and no step
    // jumps, for either curve. Largest step printed by: ./Builds/DspTests "Drive has no hole in its travel" -s | grep "largest"
    const auto input = sine (1000.0, 0.25f, 4800);
    for (int curve : { symmetric, asymmetric })
    {
        double previous = 0.0, largestStepDb = 0.0;
        for (float db = 0.0f; db <= 24.0f; db += 0.5f)
        {
            InputDrive drive;
            prepared (drive, db, curve);
            auto output = input;
            run (drive, output);
            const double level = rms (std::span<const float> { output }.subspan (2400));
            INFO ("curve " << curve << ", " << db << " dB, RMS " << level);
            CHECK (level >= previous);
            if (previous > 0.0)
                largestStepDb = std::max (largestStepDb, 20.0 * std::log10 (level / previous));
            previous = level;
        }
        SUCCEED ("largest step curve " << curve << ": " << largestStepDb << " dB");
        CHECK (largestStepDb < 1.5);
    }
}

TEST_CASE ("A full scale signal barely dips at low Drive", "[dsp][drive]")
{
    // 0 dBFS sine: saturation lowers the peaks before the gain makes up for it, so the level may dip
    // slightly under the dry level. Printed by: ./Builds/DspTests "A full scale signal barely dips at low Drive" -s | grep "dip"
    const auto input = sine (1000.0, 1.0f, 4800);
    const double dry = rms (std::span<const float> { input }.subspan (2400));
    for (int curve : { symmetric, asymmetric })
    {
        double lowest = dry;
        for (float db = 0.0f; db <= 24.0f; db += 0.5f)
        {
            InputDrive drive;
            prepared (drive, db, curve);
            auto output = input;
            run (drive, output);
            lowest = std::min (lowest, rms (std::span<const float> { output }.subspan (2400)));
        }
        const double dipDb = 20.0 * std::log10 (lowest / dry);
        SUCCEED ("dip curve " << curve << ": " << dipDb << " dB");
        CHECK (dipDb > -0.5);
    }
}

TEST_CASE ("The asymmetric curve limits as much in the bass", "[dsp][drive]")
{
    // Code review of task 0013: 0 dBFS sine, 24 dB, last second of 2 s. The curve's own range is -1.41
    // to 0.77 (-1 / (1 - 0.2913), 1 / (1 + 0.2913)), a span of 2.18. A DC blocker on the whole curve
    // overshoots in the bass (3.3 at 20 Hz, 2.9 at 40 Hz); on the even part alone it does not. Printed by: ./Builds/DspTests "The asymmetric curve limits as much in the bass" -s | grep "span"
    for (double frequency : { 20.0, 40.0, 1000.0 })
    {
        auto output = sine (frequency, 1.0f, 96000);
        InputDrive drive;
        prepared (drive, 24.0f, asymmetric);
        run (drive, output);
        const auto [low, high] = std::minmax_element (output.begin() + 48000, output.end());
        SUCCEED ("span at " << frequency << " Hz: " << *high - *low);
        CHECK (*high - *low < 2.3f);
    }
}

TEST_CASE ("The asymmetric curve adds even harmonics, the symmetric one does not", "[dsp][drive]")
{
    // ADR 0009. 1 kHz sine at -12 dBFS, 12 dB, second harmonic against the fundamental over the last
    // 1 s of 2 s. Printed by: ./Builds/DspTests "The asymmetric curve adds even harmonics\, the symmetric one does not" -s | grep "second"
    const auto input = sine (1000.0, 0.25f, 96000);
    auto secondHarmonicDb = [&] (int curve) {
        InputDrive drive;
        prepared (drive, 12.0f, curve);
        auto output = input;
        run (drive, output);
        const std::span<const float> last = std::span<const float> { output }.subspan (48000);
        return 20.0 * std::log10 (std::max (amplitude (last, 2000.0), 1e-12) / amplitude (last, 1000.0));
    };
    const auto symmetricDb = secondHarmonicDb (symmetric), asymmetricDb = secondHarmonicDb (asymmetric);
    SUCCEED ("second harmonic symmetric " << symmetricDb << " dB, asymmetric " << asymmetricDb << " dB");
    CHECK (symmetricDb < -100.0);
    CHECK (asymmetricDb > -50.0);
}

TEST_CASE ("The asymmetric curve leaves no DC offset", "[dsp][drive]")
{
    // ADR 0009. 1 kHz sine at -6 dBFS, 24 dB: mean over the last 1 s of 2 s, a whole number of
    // periods. Printed by: ./Builds/DspTests "The asymmetric curve leaves no DC offset" -s | grep "mean"
    // With `blocked = even` in InputDrive.cpp: -0.29. As it is: 7e-7.
    auto output = sine (1000.0, 0.5f, 96000);
    InputDrive drive;
    prepared (drive, 24.0f, asymmetric);
    run (drive, output);
    double sum = 0.0;
    for (std::size_t i = 48000; i < output.size(); ++i)
        sum += static_cast<double> (output[i]);
    const double mean = sum / 48000.0;
    SUCCEED ("mean " << mean);
    CHECK (std::abs (mean) < 1e-3);
}

TEST_CASE ("Drive adds nothing to silence", "[dsp][drive]")
{
    for (int curve : { symmetric, asymmetric })
    {
        InputDrive drive;
        prepared (drive, 24.0f, curve);
        std::vector<float> output (4096, 0.0f);
        run (drive, output);
        for (auto s : output)
            REQUIRE (std::abs (s) < 1e-9f);
    }
}

TEST_CASE ("A Drive change ramps over 20 ms", "[dsp][drive]")
{
    // SPEC: Drive smoothed, same ramp time as Gain out. Symmetric, memoryless: from the change +
    // 960 samples the output is the one of the target, and one sample before it is not.
    constexpr int change = 2048;
    const auto input = sine (1000.0, 0.25f, 8192);
    InputDrive drive;
    prepared (drive, 0.0f, symmetric);
    auto output = input;
    std::span<float> all { output };
    run (drive, all.first (change));
    drive.setDriveDb (24.0f);
    run (drive, all.subspan (change));

    InputDrive target;
    prepared (target, 24.0f, symmetric);
    auto expected = input;
    run (target, expected);

    for (std::size_t i = change + rampLength; i < output.size(); ++i)
        REQUIRE (std::abs (output[i] - expected[i]) < 1e-6f);
    // Nothing moves before the change
    for (std::size_t i = 0; i < change; ++i)
        REQUIRE (std::abs (output[i] - input[i]) < 1e-7f);
}

TEST_CASE ("A change between two Drive values ramps the gain over 20 ms", "[dsp][drive]")
{
    // Task 0013 verifier: 6 to 24 dB, where the blend is already all saturated, so only the gain moves.
    // Constant 0.01, in the near linear part of tanh, so the output follows the gain. The ramp lands on
    // its last sample, change + 959 (sample 960 of the ramp), within the row's 1 sample.
    constexpr int change = 2048;
    InputDrive drive;
    prepared (drive, 6.0f, symmetric);
    std::vector<float> output (8192, 0.01f);
    std::span<float> all { output };
    run (drive, all.first (change));
    drive.setDriveDb (24.0f);
    run (drive, all.subspan (change));

    InputDrive target;
    prepared (target, 24.0f, symmetric);
    std::vector<float> expected (8192, 0.01f);
    run (target, expected);
    for (std::size_t i = change + rampLength - 1; i < output.size(); ++i)
        REQUIRE (std::abs (output[i] - expected[i]) < 1e-6f);
    CHECK (std::abs (output[change + rampLength - 3] - expected[change + rampLength - 3]) > 1e-5f);
    // Half way, half way: 0.01 x (2 + 15.85) / 2, within tanh's curve at this level
    CHECK (output[change + rampLength / 2] == Approx (0.0892).margin (2e-3));
}

TEST_CASE ("A Drive curve change blends over 20 ms", "[dsp][drive]")
{
    // No reset, and the DC blockers run whatever the curve: from the change + 959 samples the output
    // is exactly the one of the target curve, and two samples before it is not (task 0013 verifier).
    constexpr int change = 2048;
    const auto input = sine (1000.0, 0.25f, 8192);
    InputDrive drive;
    prepared (drive, 24.0f, symmetric);
    auto output = input;
    std::span<float> all { output };
    run (drive, all.first (change));
    drive.setCurveIndex (asymmetric);
    run (drive, all.subspan (change));

    InputDrive target;
    prepared (target, 24.0f, asymmetric);
    auto expected = input;
    run (target, expected);
    for (std::size_t i = change + rampLength - 1; i < output.size(); ++i)
        REQUIRE (std::abs (output[i] - expected[i]) < 1e-6f);
    CHECK (std::abs (output[change + rampLength - 3] - expected[change + rampLength - 3]) > 1e-5f);
}

TEST_CASE ("The DC blocker keeps no non-finite value", "[dsp][drive]")
{
    // .claude/rules/dsp.md: one NaN or infinite input sample must not poison the later samples. The
    // whole plugin is another matter: the filter keeps it (FUTURE.md).
    for (float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
    {
        InputDrive drive;
        prepared (drive, 12.0f, asymmetric);
        auto output = sine (1000.0, 0.25f, 2048);
        output[100] = bad;
        run (drive, output);
        for (std::size_t i = 101; i < output.size(); ++i)
            REQUIRE (std::isfinite (output[i]));
    }
}

TEST_CASE ("Huge finite samples leave nothing behind in the input drive", "[dsp][drive]")
{
    // rt-safety review of task 0013: +3e38 then -3e38 stuck in the blocker when it took the whole
    // curve minus the input; the even part it takes now is bounded (at most 0.32 in magnitude)
    InputDrive drive;
    prepared (drive, 24.0f, asymmetric);
    auto output = sine (1000.0, 0.25f, 4096);
    output[100] = 3e38f;
    output[101] = -3e38f;
    run (drive, output);
    for (std::size_t i = 102; i < output.size(); ++i)
        REQUIRE (std::isfinite (output[i]));
}

TEST_CASE ("Each channel is driven alike and on its own", "[dsp][drive]")
{
    // SPEC: linked stereo, one gain; the DC blockers stay separate. Each channel of a stereo run
    // equals the same signal driven alone, and silence next to a signal stays silence.
    auto driven = [] (std::vector<float> samples) {
        InputDrive alone;
        prepared (alone, 24.0f, asymmetric);
        run (alone, samples);
        return samples;
    };
    InputDrive drive;
    prepared (drive, 24.0f, asymmetric);
    auto left = sine (1000.0, 0.25f, 4096);
    auto right = sine (300.0, 0.125f, 4096);
    const auto expectedLeft = driven (left), expectedRight = driven (right);
    const std::array<std::span<float>, 2> channels { std::span<float> { left }, std::span<float> { right } };
    drive.process (channels, 4096);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        REQUIRE (std::abs (left[i] - expectedLeft[i]) < 1e-7f);
        REQUIRE (std::abs (right[i] - expectedRight[i]) < 1e-7f);
    }

    InputDrive next;
    prepared (next, 24.0f, asymmetric);
    auto signal = sine (1000.0, 0.25f, 4096);
    std::vector<float> silence (4096, 0.0f);
    const std::array<std::span<float>, 2> mixed { std::span<float> { signal }, std::span<float> { silence } };
    next.process (mixed, 4096);
    for (auto sample : silence)
        REQUIRE (std::abs (sample) < 1e-7f);
}

TEST_CASE ("Drive values out of range are bounded", "[dsp][drive]")
{
    CHECK (firstSample (99.0f, symmetric, 0.001f) == Approx (firstSample (24.0f, symmetric, 0.001f)).margin (1e-7));
    CHECK (firstSample (-9.0f, symmetric, 0.25f) == Approx (0.25f).margin (1e-7));
    CHECK (firstSample (12.0f, 7, 0.5f) == Approx (firstSample (12.0f, asymmetric, 0.5f)).margin (1e-7));
    CHECK (firstSample (12.0f, -3, 0.5f) == Approx (firstSample (12.0f, symmetric, 0.5f)).margin (1e-7));
    InputDrive drive;
    prepared (drive, 0.0f, symmetric);
    drive.setDriveDb (std::numeric_limits<float>::quiet_NaN());
    drive.reset();
    std::array<float, 1> sample { 0.25f };
    run (drive, sample);
    CHECK (sample[0] == Approx (0.25f).margin (1e-7));
}
