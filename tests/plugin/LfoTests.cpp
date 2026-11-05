#include "plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace
{
    template <typename Parameter>
    Parameter& parameter (PluginProcessor& processor, const juce::String& id)
    {
        for (auto* p : processor.getParameters())
            if (auto* typed = dynamic_cast<Parameter*> (p); typed != nullptr && typed->paramID == id)
                return *typed;
        FAIL ("no parameter with ID " << id);
        throw std::logic_error ("unreachable");
    }

    juce::AudioParameterFloat& floatParameter (PluginProcessor& p, const juce::String& id) { return parameter<juce::AudioParameterFloat> (p, id); }
    juce::AudioParameterChoice& choiceParameter (PluginProcessor& p, const juce::String& id) { return parameter<juce::AudioParameterChoice> (p, id); }

    constexpr double sampleRate = 48000.0;
    constexpr int sine = 0, square = 3;
    constexpr int fourBars = 0, half = 9, quarter = 12;

    // A host transport: the test moves it before each block
    struct TestPlayHead : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override
        {
            if (!hasPosition)
                return {};
            PositionInfo info;
            if (hasTempo)
                info.setBpm (bpm);
            info.setPpqPosition (quarterNotes);
            info.setIsPlaying (playing);
            return info;
        }

        bool hasPosition = true;
        bool playing = true;
        bool hasTempo = true;
        double bpm = 120.0;
        double quarterNotes = 0.0;
    };

    // Sine of frequency, peak: left output of the whole render. onBlock runs before each block, with the
    // sample at which that block starts. The play head, if any, follows the samples at its tempo.
    std::vector<float> render (PluginProcessor& processor, TestPlayHead* head, int total, int blockSize, double frequency, float peak, const std::function<void (int)>& onBlock = {}, double startQuarterNotes = 0.0)
    {
        processor.setPlayHead (head);
        processor.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        std::vector<float> output;
        output.reserve (static_cast<std::size_t> (total));
        if (head != nullptr)
            head->quarterNotes = startQuarterNotes;
        for (int start = 0; start < total; start += blockSize)
        {
            if (onBlock)
                onBlock (start);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (channel, i, peak * static_cast<float> (std::sin (2.0 * std::numbers::pi * frequency * (start + i) / sampleRate)));
            processor.processBlock (buffer, midi);
            output.insert (output.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            if (head != nullptr && head->playing)
                head->quarterNotes += blockSize * head->bpm / (60.0 * sampleRate);
        }
        output.resize (static_cast<std::size_t> (total));
        return output;
    }

    void setUp (PluginProcessor& p, int shape, int rate, float depth, float cutoff = 500.0f)
    {
        choiceParameter (p, "lfoShape") = shape;
        choiceParameter (p, "lfoRate") = rate;
        floatParameter (p, "lfoDepth") = depth;
        floatParameter (p, "cutoff") = cutoff;
    }

    float largestDifference (const std::vector<float>& a, const std::vector<float>& b, std::size_t from, std::size_t to)
    {
        float largest = 0.0f;
        for (std::size_t i = from; i < to; ++i)
        {
            const float difference = std::abs (a[i] - b[i]);
            largest = std::isfinite (difference) ? std::max (largest, difference) : 1e30f;
        }
        return largest;
    }

    double rmsDb (const std::vector<float>& samples, std::size_t centre)
    {
        double sum = 0.0;
        for (std::size_t i = centre - 480; i < centre + 480; ++i)
            sum += static_cast<double> (samples[i]) * samples[i];
        return 10.0 * std::log10 (sum / 960.0);
    }

    // Largest |y[n] - 2 y[n-1] + y[n-2]| (click measure, task 0011) from the given sample
    double clickFrom (const std::vector<float>& y, std::size_t from)
    {
        double largest = 0.0;
        for (std::size_t n = std::max<std::size_t> (from, 2); n < y.size(); ++n)
            largest = std::max (largest, std::abs (static_cast<double> (y[n]) - 2.0 * y[n - 1] + y[n - 2]));
        return largest;
    }
}

TEST_CASE ("The LFO parameters match the SPEC", "[plugin][lfo]")
{
    // SPEC: shapes, 4 bars to 1/32 with triplets and dotted values, default 1/4, sine, depth 0 to +/-4 octaves
    PluginProcessor processor;
    auto& shape = choiceParameter (processor, "lfoShape");
    CHECK (shape.choices == juce::StringArray { "Sine", "Triangle", "Sawtooth", "Square" });
    CHECK (shape.getIndex() == 0);
    auto& rate = choiceParameter (processor, "lfoRate");
    CHECK (rate.choices.size() == 22);
    CHECK (rate.choices[0] == "4 bars");
    CHECK (rate.choices[21] == "1/32");
    CHECK (rate.getCurrentChoiceName() == "1/4");
    auto& depth = floatParameter (processor, "lfoDepth");
    CHECK (depth.range.start == Catch::Approx (-4.0f));
    CHECK (depth.range.end == Catch::Approx (4.0f));
    CHECK (depth.get() == Catch::Approx (0.0f));
    auto text = [&] (float value) { return static_cast<juce::AudioProcessorParameter&> (depth).getText (depth.convertTo0to1 (value), 32); };
    CHECK (text (1.5f) == "+1.50 oct");
    CHECK (text (-2.0f) == "-2.00 oct");
    CHECK (text (0.0f) == "0.00 oct");
}

TEST_CASE ("The snapshot always gives valid LFO indices", "[plugin][lfo]")
{
    using moodz::parameters::toLfoRateIndex;
    using moodz::parameters::toLfoShapeIndex;
    CHECK (toLfoShapeIndex (3.0f) == 3);
    CHECK (toLfoShapeIndex (99.0f) == 3);
    CHECK (toLfoShapeIndex (-1.0f) == 0);
    CHECK (toLfoShapeIndex (std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK (toLfoRateIndex (21.0f) == 21);
    CHECK (toLfoRateIndex (99.0f) == 21);
    CHECK (toLfoRateIndex (-1.0f) == 0);
    CHECK (toLfoRateIndex (std::numeric_limits<float>::infinity()) == 0);
}

TEST_CASE ("At depth 0 the LFO leaves the plugin as it was", "[plugin][lfo]")
{
    // Goal of task 0015: with the host's transport or without, any shape and rate
    PluginProcessor plain;
    floatParameter (plain, "cutoff") = 500.0f;
    const auto reference = render (plain, nullptr, 9600, 512, 2000.0, 0.25f);
    PluginProcessor moved;
    setUp (moved, square, 21, 0.0f);
    TestPlayHead head;
    CHECK (largestDifference (render (moved, &head, 9600, 512, 2000.0, 0.25f), reference, 0, reference.size()) < 1e-7f);
}

TEST_CASE ("The LFO moves the plugin output", "[plugin][lfo]")
{
    // SPEC, post-mortem 0013: the effect must arrive. 2 kHz sine, sine LFO on a 1/4 at 120 BPM from beat 0,
    // depth +2 octaves on a cutoff of 500 Hz: loud at phase 1/4 (sample 30000 + 40) and quiet at phase 3/4
    // (42000 + 40). Printed by: ./Builds/PluginTests "The LFO moves the plugin output" -s | grep "swing"
    auto swing = [] (float depth) {
        PluginProcessor processor;
        setUp (processor, sine, quarter, depth);
        TestPlayHead head;
        const auto output = render (processor, &head, 48000, 512, 2000.0, 0.25f);
        return rmsDb (output, 30040) - rmsDb (output, 42040);
    };
    const double up = swing (2.0f), down = swing (-2.0f);
    SUCCEED ("swing " << up << " dB, " << down << " dB");
    CHECK (up > 20.0);
    CHECK (down < -20.0);
}

TEST_CASE ("LFO changes during playback reach the output", "[plugin][lfo]")
{
    // Post-mortem 0013. Each change moved at block 20 is compared with the run where it does not move
    auto outputWith = [] (const std::function<void (PluginProcessor&)>& change) {
        PluginProcessor processor;
        setUp (processor, sine, quarter, 2.0f);
        TestPlayHead head;
        return render (processor, &head, 24000, 512, 2000.0, 0.25f, [&] (int start) { if (start == 20 * 512) change (processor); });
    };
    const auto reference = outputWith ([] (PluginProcessor&) {});
    for (const auto* id : { "lfoShape", "lfoRate", "lfoDepth" })
    {
        INFO (id);
        const auto moved = outputWith ([&] (PluginProcessor& p) {
            if (juce::String (id) == "lfoShape")
                choiceParameter (p, id) = square;
            else if (juce::String (id) == "lfoRate")
                choiceParameter (p, id) = half;
            else
                floatParameter (p, id) = -2.0f;
        });
        CHECK (largestDifference (moved, reference, 0, 20 * 512) < 1e-6f);
        CHECK (largestDifference (moved, reference, 20 * 512, moved.size()) > 0.01f);
    }
}

TEST_CASE ("The plugin with the LFO does not depend on the block size", "[plugin][lfo]")
{
    // ROADMAP: the block size test with the LFO active, on a host position moving regularly. Square on
    // a 1/16 at 120 BPM: an edge falls exactly on the modulation grid at sample 12000, where the rounding
    // of the phase once put it on a side that depended on the block size.
    auto renderAt = [] (int blockSize) {
        PluginProcessor processor;
        setUp (processor, square, 18, 3.0f, 1000.0f);
        TestPlayHead head;
        return render (processor, &head, 32768, blockSize, 300.0, 0.25f);
    };
    const auto reference = renderAt (2048);
    for (int blockSize : { 64, 512 })
    {
        INFO ("block size " << blockSize);
        CHECK (largestDifference (renderAt (blockSize), reference, 0, reference.size()) < 1e-5f);
    }
}

TEST_CASE ("The LFO is locked to the song position", "[plugin][lfo]")
{
    // Goal of task 0015. Playback started at beat 9 against the same beat reached from beat 0: on a 1/2 note
    // that is half a cycle, so a free running LFO would be on the opposite side. 200 Hz has a whole number
    // of periods per beat at 120 BPM, so the input is the same too. Compared once
    // the filter has settled. Printed by: ./Builds/PluginTests "The LFO is locked to the song position" -s | grep "difference"
    constexpr int beat = 24000;
    PluginProcessor fromZero;
    setUp (fromZero, sine, half, 2.0f, 1000.0f);
    TestPlayHead zeroHead;
    const auto full = render (fromZero, &zeroHead, 12 * beat, 512, 200.0, 0.25f);

    PluginProcessor fromNine;
    setUp (fromNine, sine, half, 2.0f, 1000.0f);
    TestPlayHead nineHead;
    const auto late = render (fromNine, &nineHead, 3 * beat, 512, 200.0, 0.25f, {}, 9.0);

    const std::vector<float> tail (full.begin() + 9 * beat, full.end());
    const float difference = largestDifference (late, tail, static_cast<std::size_t> (beat), late.size());
    SUCCEED ("largest difference " << difference);
    CHECK (difference < 1e-3f);
}

TEST_CASE ("A stopped host or no play head lets the LFO run on", "[plugin][lfo]")
{
    // Decision notes: free running from where it is, at the last known tempo, 120 BPM at first.
    // Sine on a 1/4 at 120 BPM: loud at sample 30040, quiet at 42040.
    auto swing = [] (TestPlayHead* head) {
        PluginProcessor processor;
        setUp (processor, sine, quarter, 2.0f);
        const auto output = render (processor, head, 48000, 512, 2000.0, 0.25f);
        return rmsDb (output, 30040) - rmsDb (output, 42040);
    };
    // Printed by the test: ./Builds/PluginTests "The LFO moves the plugin output" -s | grep "swing" gives the same level
    CHECK (swing (nullptr) > 20.0);
    TestPlayHead stopped;
    stopped.playing = false;
    CHECK (swing (&stopped) > 20.0);
    TestPlayHead noPosition;
    noPosition.hasPosition = false;
    CHECK (swing (&noPosition) > 20.0);
    TestPlayHead noTempo;
    noTempo.hasTempo = false;
    noTempo.playing = false;
    CHECK (swing (&noTempo) > 20.0);
}

TEST_CASE ("The LFO follows the host tempo", "[plugin][lfo]")
{
    // SPEC. 60 BPM, stopped so the tempo alone sets the speed: a 1/4 lasts 48000 samples, loud at 60040
    // (phase 1/4 of the second cycle), quiet at 84040. Then moved to 240 BPM during the run.
    PluginProcessor slow;
    setUp (slow, sine, quarter, 2.0f);
    TestPlayHead slowHead;
    slowHead.playing = false;
    slowHead.bpm = 60.0;
    const auto output = render (slow, &slowHead, 96000, 512, 2000.0, 0.25f);
    const double slowSwing = rmsDb (output, 60040) - rmsDb (output, 84040);
    SUCCEED ("swing at 60 BPM " << slowSwing << " dB");
    CHECK (slowSwing > 20.0);

    PluginProcessor moved;
    setUp (moved, sine, quarter, 2.0f);
    TestPlayHead movedHead;
    movedHead.playing = false;
    movedHead.bpm = 60.0;
    const auto after = render (moved, &movedHead, 96000, 512, 2000.0, 0.25f, [&] (int start) { if (start == 48000 - 48000 % 512) movedHead.bpm = 240.0; });
    // The block at 47616 still has 0.008 of a cycle to go, 96 samples at 240 BPM: phase 0 at 47712, then
    // 12000 samples per cycle, loud at 47712 + 3000 + 40 and quiet at 47712 + 9000 + 40. Printed by:
    // ./Builds/PluginTests "The LFO follows the host tempo" -s | grep "swing"
    const double movedSwing = rmsDb (after, 50752) - rmsDb (after, 56752);
    SUCCEED ("swing after the tempo moved " << movedSwing << " dB");
    CHECK (movedSwing > 20.0);

    // Playing: the host's position moves at the tempo, and the LFO with it. 240 BPM from sample 0: a 1/4
    // lasts 12000 samples, loud at 15040 and quiet at 21040.
    PluginProcessor playing;
    setUp (playing, sine, quarter, 2.0f);
    TestPlayHead playingHead;
    playingHead.bpm = 240.0;
    const auto fast = render (playing, &playingHead, 48000, 512, 2000.0, 0.25f);
    const double fastSwing = rmsDb (fast, 15040) - rmsDb (fast, 21040);
    SUCCEED ("swing at 240 BPM while playing " << fastSwing << " dB");
    CHECK (fastSwing > 20.0);
}

TEST_CASE ("The LFO keeps the last tempo when the host stops giving one", "[plugin][lfo]")
{
    // Decision notes of task 0015. 60 BPM for the first block, then no tempo: a 1/4 lasts 48000 samples,
    // loud at 60040 and quiet at 84040; at 120 BPM these two samples would be the same phase.
    PluginProcessor processor;
    setUp (processor, sine, quarter, 2.0f);
    TestPlayHead head;
    head.playing = false;
    head.bpm = 60.0;
    const auto output = render (processor, &head, 96000, 512, 2000.0, 0.25f, [&] (int start) {
        if (start == 512)
            head.hasTempo = false;
    });
    CHECK (rmsDb (output, 60040) - rmsDb (output, 84040) > 20.0);
}

TEST_CASE ("A session with a depth starts at that depth", "[plugin][lfo]")
{
    // Task 0015 verifier, row "prepared, then playing": no ramp from 0 at the start. Square on a 1/2 at beat 1,
    // value -1, depth 4 octaves: 2 kHz sine at -12 dBFS under a cutoff of 62.5 Hz from the start. The first
    // window holds the sine's onset, so it is above the steady level, but far under what a ramp from depth
    // 0 lets through (cutoff 1 kHz). Printed by:
    // ./Builds/PluginTests "A session with a depth starts at that depth" -s | grep "level"
    PluginProcessor processor;
    setUp (processor, square, half, 4.0f, 1000.0f);
    TestPlayHead head;
    const auto output = render (processor, &head, 9600, 512, 2000.0, 0.25f, {}, 1.0);
    const double first = rmsDb (output, 600), later = rmsDb (output, 6000);
    SUCCEED ("level first " << first << " dB, later " << later << " dB");
    CHECK (first < -50.0);
    CHECK (later < -70.0);
}

TEST_CASE ("LFO edges and jumps stay curves", "[plugin][lfo][click]")
{
    // SPEC success criterion 3. 200 Hz sine, Cytomic SVF Low-pass at 1 kHz, resonance 20 %, depth +4 and -4
    // octaves: a square edge, a sawtooth reset, a position jump (a loop) and a Rate change at -12 and 0 dBFS.
    // Without the slew each jump is 8 octaves in one step. Upward edges measure the most: a square, and a
    // sawtooth reset at -4 octaves. Maxime heard the square at +/-4 octaves in Live on 2026-11-05 and did
    // not object. Click measures printed by:
    // ./Builds/PluginTests "LFO edges and jumps stay curves" -s | grep "click"
    for (float depth : { 4.0f, -4.0f })
        for (float peak : { 0.25f, 1.0f })
        {
            auto measured = [&] (int shape, int rate, const std::function<void (PluginProcessor&, TestPlayHead&, int)>& onBlock) {
                PluginProcessor processor;
                setUp (processor, shape, rate, depth, 1000.0f);
                TestPlayHead head;
                const auto output = render (processor, &head, 48000, 512, 200.0, peak, [&] (int start) { onBlock (processor, head, start); });
                return clickFrom (output, 2400);
            };
            const auto nothing = [] (PluginProcessor&, TestPlayHead&, int) {};
            const auto squareEdges = measured (square, quarter, nothing);
            const auto sawtoothResets = measured (2, quarter, nothing);
            const auto loop = measured (sine, quarter, [] (PluginProcessor&, TestPlayHead& head, int start) {
                if (start == 24 * 512)
                {
                    REQUIRE (head.quarterNotes > 0.5);
                    head.quarterNotes -= 0.25;
                }
            });
            const auto shapeChange = measured (sine, quarter, [] (PluginProcessor& p, TestPlayHead&, int start) {
                if (start == 24 * 512)
                    choiceParameter (p, "lfoShape") = square;
            });
            const auto rateChange = measured (sine, fourBars, [] (PluginProcessor& p, TestPlayHead&, int start) {
                if (start == 24 * 512)
                    choiceParameter (p, "lfoRate") = 12;
            });
            SUCCEED ("click depth " << depth << ", peak " << peak << ": square " << squareEdges << ", sawtooth " << sawtoothResets << ", loop " << loop << ", rate " << rateChange << ", shape " << shapeChange);
            CHECK (squareEdges < 0.004 * peak);
            CHECK (sawtoothResets < 0.004 * peak);
            CHECK (shapeChange < 0.004 * peak);
            CHECK (loop < 0.004 * peak);
            CHECK (rateChange < 0.004 * peak);
        }
}

TEST_CASE ("A bad host transport stays harmless", "[plugin][lfo]")
{
    // .claude/rules/dsp.md: NaN and infinite tempo or position from a host
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto infinity = std::numeric_limits<double>::infinity();
    for (double bad : { nan, infinity, -infinity })
    {
        INFO ("value " << bad);
        PluginProcessor processor;
        setUp (processor, sine, quarter, 2.0f);
        TestPlayHead head;
        head.bpm = bad;
        const auto badTempo = render (processor, &head, 9600, 512, 2000.0, 0.25f, [&] (int start) {
            if (start == 2048)
                head.bpm = 120.0;
        });
        CHECK (std::all_of (badTempo.begin(), badTempo.end(), [] (float s) { return std::isfinite (s); }));
        const auto badPosition = render (processor, &head, 9600, 512, 2000.0, 0.25f, [&] (int start) {
            if (start == 2048)
                head.quarterNotes = bad;
            if (start == 4096)
                head.quarterNotes = 1.0;
        });
        CHECK (std::all_of (badPosition.begin(), badPosition.end(), [] (float s) { return std::isfinite (s); }));
    }
}

TEST_CASE ("A state saved before the LFO loads with its defaults", "[plugin][lfo][state]")
{
    // .claude/rules/juce.md: the missing values fall back to Sine, 1/4, depth 0; others are kept
    juce::XmlElement xml ("MoodzFilter");
    xml.setAttribute ("stateVersion", 1);
    auto* saved = xml.createNewChildElement ("PARAM");
    saved->setAttribute ("id", "cutoff");
    saved->setAttribute ("value", 5000.0);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (xml, block);

    PluginProcessor processor;
    setUp (processor, square, 0, 3.0f);
    processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK (choiceParameter (processor, "lfoShape").getIndex() == 0);
    CHECK (choiceParameter (processor, "lfoRate").getCurrentChoiceName() == "1/4");
    CHECK (floatParameter (processor, "lfoDepth").get() == Catch::Approx (0.0f));
    CHECK (floatParameter (processor, "cutoff").get() == Catch::Approx (5000.0f).margin (0.01));
}

TEST_CASE ("The LFO moving with everything else stays finite", "[plugin][lfo]")
{
    // Under RealtimeSanitizer in CI. 2 s of noise, resonance 100 %, a moving host position, every LFO
    // parameter and the ones that restart a path changing on every block.
    PluginProcessor processor;
    floatParameter (processor, "resonance") = 100.0f;
    TestPlayHead head;
    processor.setPlayHead (&head);
    processor.prepareToPlay (sampleRate, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    juce::Random random (47);
    for (int block = 0; block < 188; ++block)
    {
        choiceParameter (processor, "lfoShape") = block % 4;
        choiceParameter (processor, "lfoRate") = block % 22;
        floatParameter (processor, "lfoDepth") = -4.0f + static_cast<float> (block % 9);
        floatParameter (processor, "cutoff") = block % 2 == 0 ? 20.0f : 20000.0f;
        choiceParameter (processor, "filterModel") = block % 9;
        choiceParameter (processor, "oversampling") = block % 3;
        floatParameter (processor, "drive") = static_cast<float> (block % 5) * 6.0f;
        head.quarterNotes = block % 50 == 0 ? 0.0 : block * 0.021;
        head.bpm = block % 7 == 0 ? 60.0 : 180.0;
        head.playing = block % 11 != 0;
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (channel, i, 0.25f * (random.nextFloat() * 2.0f - 1.0f));
        processor.processBlock (buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 512; ++i)
                REQUIRE (std::isfinite (buffer.getSample (channel, i)));
    }
}
