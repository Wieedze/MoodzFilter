#include "dsp/SyncedLfo.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>

using Catch::Approx;
using moodz::dsp::SyncedLfo;

namespace
{
    constexpr int sine = 0, triangle = 1, sawtooth = 2, square = 3;
    constexpr int quarter = SyncedLfo::defaultRateIndex;

    SyncedLfo prepared (int shape, int rate = quarter)
    {
        SyncedLfo lfo;
        lfo.prepare (48000.0);
        lfo.setShapeIndex (shape);
        lfo.setRateIndex (rate);
        lfo.reset();
        return lfo;
    }

    // The value at a phase, reached through the song position on a 1/4: 1 quarter note per cycle
    float at (int shape, double phase)
    {
        auto lfo = prepared (shape);
        lfo.syncToPosition (phase);
        return lfo.value();
    }
}

TEST_CASE ("The LFO shapes follow their definitions", "[dsp][lfo]")
{
    // Definitions in docs/tasks/0015-synced-lfo.md
    const double s = std::numbers::sqrt2 / 2.0;
    CHECK (at (sine, 0.0) == Approx (0.0).margin (1e-6));
    CHECK (at (sine, 0.125) == Approx (s).margin (1e-6));
    CHECK (at (sine, 0.25) == Approx (1.0).margin (1e-6));
    CHECK (at (sine, 0.5) == Approx (0.0).margin (1e-6));
    CHECK (at (sine, 0.75) == Approx (-1.0).margin (1e-6));

    CHECK (at (triangle, 0.0) == Approx (0.0).margin (1e-6));
    CHECK (at (triangle, 0.125) == Approx (0.5).margin (1e-6));
    CHECK (at (triangle, 0.25) == Approx (1.0).margin (1e-6));
    CHECK (at (triangle, 0.5) == Approx (0.0).margin (1e-6));
    CHECK (at (triangle, 0.75) == Approx (-1.0).margin (1e-6));
    CHECK (at (triangle, 0.875) == Approx (-0.5).margin (1e-6));

    CHECK (at (sawtooth, 0.0) == Approx (-1.0).margin (1e-6));
    CHECK (at (sawtooth, 0.25) == Approx (-0.5).margin (1e-6));
    CHECK (at (sawtooth, 0.5) == Approx (0.0).margin (1e-6));
    CHECK (at (sawtooth, 0.75) == Approx (0.5).margin (1e-6));

    CHECK (at (square, 0.0) == Approx (1.0).margin (1e-6));
    CHECK (at (square, 0.25) == Approx (1.0).margin (1e-6));
    CHECK (at (square, 0.5) == Approx (-1.0).margin (1e-6));
    CHECK (at (square, 0.75) == Approx (-1.0).margin (1e-6));
}

TEST_CASE ("Every rate lasts its length in quarter notes", "[dsp][lfo]")
{
    // SPEC. At 120 BPM a quarter note lasts 24000 samples at 48 kHz. Advanced in blocks of 512, as the
    // phase must not depend on them: back at 0 after the cycle, within 1 sample.
    REQUIRE (SyncedLfo::rates.size() == 22);
    for (int rate = 0; rate < static_cast<int> (SyncedLfo::rates.size()); ++rate)
    {
        const double quarters = SyncedLfo::rates[static_cast<std::size_t> (rate)].quarterNotes;
        INFO (SyncedLfo::rates[static_cast<std::size_t> (rate)].label);
        auto lfo = prepared (sawtooth, rate);
        const int cycle = static_cast<int> (std::lround (quarters * 24000.0));
        lfo.advance (cycle / 2);
        CHECK (lfo.phase() == Approx (0.5).margin (1.0 / cycle));
        int left = cycle - cycle / 2;
        while (left > 0)
        {
            const int length = std::min (512, left);
            lfo.advance (length);
            left -= length;
        }
        const double distance = std::min (lfo.phase(), 1.0 - lfo.phase());
        CHECK (distance * cycle < 1.0);
    }
}

TEST_CASE ("The rates are the frozen list", "[dsp][lfo]")
{
    // SPEC: 4 bars to 1/32, with triplets and dotted values. Lengths in docs/tasks/0015-synced-lfo.md
    CHECK (std::string (SyncedLfo::rates.front().label) == "4 bars");
    CHECK (SyncedLfo::rates.front().quarterNotes == Approx (16.0));
    CHECK (std::string (SyncedLfo::rates.back().label) == "1/32");
    CHECK (SyncedLfo::rates.back().quarterNotes == Approx (0.125));
    CHECK (std::string (SyncedLfo::rates[quarter].label) == "1/4");
    CHECK (SyncedLfo::rates[quarter].quarterNotes == Approx (1.0));
    for (std::size_t i = 1; i < SyncedLfo::rates.size(); ++i)
    {
        INFO (SyncedLfo::rates[i].label);
        CHECK (SyncedLfo::rates[i].quarterNotes < SyncedLfo::rates[i - 1].quarterNotes);
    }
}

TEST_CASE ("The phase comes from the song position", "[dsp][lfo]")
{
    // Decision notes: p = frac (position / length), on a 1/2 (2 quarter notes)
    constexpr int half = 9;
    REQUIRE (SyncedLfo::rates[half].quarterNotes == Approx (2.0));
    auto lfo = prepared (sawtooth, half);
    for (double position : { 0.0, 2.0, 8.0, 2048.0, -2.0, -8.0 })
    {
        INFO ("position " << position);
        lfo.syncToPosition (position);
        CHECK (lfo.phase() == Approx (0.0).margin (1e-9));
    }
    lfo.syncToPosition (1.0);
    CHECK (lfo.phase() == Approx (0.5).margin (1e-9));
    lfo.syncToPosition (-0.5);
    CHECK (lfo.phase() == Approx (0.75).margin (1e-9));
    lfo.syncToPosition (2049.5);
    CHECK (lfo.phase() == Approx (0.75).margin (1e-9));
}

TEST_CASE ("The tempo sets the speed", "[dsp][lfo]")
{
    auto lfo = prepared (sawtooth);
    lfo.setTempoBpm (60.0); // a quarter note lasts 48000 samples
    lfo.advance (12000);
    CHECK (lfo.phase() == Approx (0.25).margin (1e-9));
    lfo.setTempoBpm (240.0); // 12000 samples
    lfo.advance (3000);
    CHECK (lfo.phase() == Approx (0.5).margin (1e-9));
}

TEST_CASE ("A bad tempo or position is ignored", "[dsp][lfo]")
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto infinity = std::numeric_limits<double>::infinity();
    auto lfo = prepared (sawtooth);
    // 120 BPM at first
    lfo.advance (6000);
    CHECK (lfo.phase() == Approx (0.25).margin (1e-9));
    for (double bad : { 0.0, -120.0, nan, infinity, -infinity })
    {
        INFO ("tempo " << bad);
        lfo.syncToPosition (0.0);
        lfo.setTempoBpm (bad);
        lfo.advance (6000);
        CHECK (lfo.phase() == Approx (0.25).margin (1e-9));
    }
    for (double bad : { nan, infinity, -infinity })
    {
        INFO ("position " << bad);
        lfo.syncToPosition (0.25);
        lfo.syncToPosition (bad);
        CHECK (lfo.phase() == Approx (0.25).margin (1e-9));
    }
}

TEST_CASE ("LFO indices out of range are bounded", "[dsp][lfo]")
{
    auto lfo = prepared (99, 99);
    lfo.syncToPosition (0.0625);
    // Square on the shortest rate: 0.0625 is half of 1/32
    CHECK (lfo.value() == Approx (-1.0).margin (1e-6));
    lfo.setShapeIndex (-5);
    lfo.setRateIndex (-5);
    lfo.syncToPosition (4.0);
    // Sine on the longest rate: a quarter of its cycle
    CHECK (lfo.value() == Approx (1.0).margin (1e-6));
}

TEST_CASE ("A very long run stays within one cycle", "[dsp][lfo]")
{
    auto lfo = prepared (sine);
    for (int i = 0; i < 100000; ++i)
        lfo.advance (512);
    CHECK (lfo.phase() >= 0.0);
    CHECK (lfo.phase() < 1.0);
    // Exactly reset to 0 by a sync, not drifted
    lfo.syncToPosition (0.0);
    CHECK (lfo.value() == Approx (0.0).margin (1e-6));
}
