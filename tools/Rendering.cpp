#include "Rendering.h"

#include "dsp/SmoothedGain.h"
#include "plugin/Parameters.h"
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <stdexcept>

namespace moodz::tools
{
    namespace
    {
        juce::AudioParameterFloat& gainOutParameter (PluginProcessor& processor)
        {
            for (auto* parameter : processor.getParameters())
                if (auto* floatParameter = dynamic_cast<juce::AudioParameterFloat*> (parameter))
                    if (floatParameter->paramID == parameters::id::gainOut)
                        return *floatParameter;
            throw std::runtime_error ("gainOut parameter not found");
        }
    }

    std::vector<RenderCase> renderCases()
    {
        return {
            { "unity", 0.0f, 0.0f, false },
            { "gain-plus6", 6.0f, 6.0f, false },
            { "gain-automation", -12.0f, 6.0f, true },
            // ADR 0004: fade to silence (the -inf floor) from the middle
            { "gain-mute", 0.0f, dsp::SmoothedGain::silenceFloorDb, true },
        };
    }

    std::vector<TestSignal> testSignals()
    {
        // Lengths are multiples of 512, so the middle falls on a block boundary for the
        // reference block size as well.
        return {
            { "impulse", impulse (12288) },
            { "sine-1k", sine (1000.0, 12288, renderSampleRate) },
            { "sweep", exponentialSweep (20.0, 20000.0, 49152, renderSampleRate) },
            { "pink-noise", pinkNoise (24576, 1u) },
        };
    }

    std::string renderFileName (const RenderCase& renderCase, const TestSignal& signal)
    {
        return renderCase.name + "__" + signal.name + ".wav";
    }

    juce::AudioBuffer<float> renderThroughPlugin (const RenderCase& renderCase, std::span<const float> input, int blockSize)
    {
        constexpr int numChannels = 2;
        const int total = static_cast<int> (input.size());
        const int changeAt = renderCase.automated ? total / 2 : -1;

        PluginProcessor processor;
        processor.setPlayConfigDetails (numChannels, numChannels, renderSampleRate, blockSize);
        gainOutParameter (processor) = renderCase.startDb;
        processor.prepareToPlay (renderSampleRate, blockSize);

        juce::AudioBuffer<float> output (numChannels, total);
        for (int channel = 0; channel < numChannels; ++channel)
            std::copy (input.begin(), input.end(), output.getWritePointer (channel));

        juce::MidiBuffer midi;
        int start = 0;
        while (start < total)
        {
            int length = std::min (blockSize, total - start);
            if (start < changeAt && changeAt < start + length)
                length = changeAt - start;
            if (start == changeAt)
                gainOutParameter (processor) = renderCase.endDb;

            juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), numChannels, start, length);
            processor.processBlock (block, midi);
            start += length;
        }

        processor.releaseResources();
        return output;
    }
}
