#pragma once

#include <juce_core/juce_core.h>

// The MoodzFilterTools subcommands. Each returns the process exit code:
// 0 success, 1 check failed, 2 usage or input error.
namespace moodz::tools
{
    inline constexpr int exitOk = 0;
    inline constexpr int exitFailed = 1;
    inline constexpr int exitUsage = 2;

    // Writes one WAV per render case and test signal into outputDirectory.
    int runRender (const juce::File& outputDirectory);

    // Compares every file listed in goldenDirectory/manifest.json against the renders.
    int runCompare (const juce::File& renderDirectory, const juce::File& goldenDirectory);

    // Fast battery through the real processor: finite output, block size independence.
    int runCheck();
}
