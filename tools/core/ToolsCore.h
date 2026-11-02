#pragma once

// JUCE-free building blocks of MoodzFilterTools: the test signals fed to the plugin and the
// error metrics used by `compare`. Kept apart so ToolsTests can check them without JUCE.

#include <cstdint>
#include <span>
#include <vector>

namespace moodz::tools
{
    inline constexpr double renderSampleRate = 48000.0;

    // All test signals peak at 0.25 (-12 dBFS), leaving room for +12 dB of gain without clipping.
    inline constexpr float signalPeak = 0.25f;

    // One sample at signalPeak, then silence.
    std::vector<float> impulse (int length);

    // Sine starting at phase 0.
    std::vector<float> sine (double frequency, int length, double sampleRate);

    // Exponential sine sweep from startFrequency to endFrequency over the whole length.
    std::vector<float> exponentialSweep (double startFrequency, double endFrequency, int length, double sampleRate);

    // Pink noise from a seeded generator, normalised to signalPeak. Same seed, same samples.
    std::vector<float> pinkNoise (int length, std::uint32_t seed);

    struct Difference
    {
        double maxAbsError = 0.0;
        // 20 * log10 (RMS of the difference); identical buffers give silenceDbfs.
        double rmsErrorDbfs = 0.0;
    };

    inline constexpr double silenceDbfs = -300.0;

    // Both spans must have the same size.
    Difference measureDifference (std::span<const float> actual, std::span<const float> expected);
}
