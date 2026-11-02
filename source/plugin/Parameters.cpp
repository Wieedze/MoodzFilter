#include "plugin/Parameters.h"

#include "dsp/SmoothedGain.h"

namespace moodz::parameters
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // SPEC v1: Gain out, -24 to +24 dB, default 0 dB, smoothed in the dsp layer
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id::gainOut, version },
            "Gain out",
            juce::NormalisableRange<float> { dsp::SmoothedGain::minDb, dsp::SmoothedGain::maxDb, 0.01f },
            0.0f,
            juce::AudioParameterFloatAttributes {}.withLabel ("dB")));

        return layout;
    }

    Snapshotter::Snapshotter (const juce::AudioProcessorValueTreeState& state)
        : gainOut (state.getRawParameterValue (id::gainOut))
    {
        jassert (gainOut != nullptr);
    }

    Params Snapshotter::snapshot() const noexcept
    {
        return Params { gainOut->load (std::memory_order_relaxed) };
    }
}
