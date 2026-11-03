#pragma once

#include "dsp/FilterModels.h"
#include "dsp/MultimodeFilter.h"
#include "dsp/Realtime.h"
#include "dsp/SmoothedGain.h"
#include "plugin/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // Audio thread: checked by RealtimeSanitizer in the RTSan build
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) noexcept MOODZ_NONBLOCKING override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Version written into the saved state; bump it when the state layout changes and
    // keep setStateInformation able to read every previous version (.claude/rules/juce.md).
    static constexpr int stateVersion = 1;

private:
    juce::AudioProcessorValueTreeState state { *this, nullptr, "MoodzFilter", moodz::parameters::createLayout() };
    moodz::parameters::Snapshotter snapshotter { state };
    moodz::dsp::SmoothedGain gainOut;

    // One prepared filter per entry of the filter list: processBlock only selects one, so
    // switching the model never allocates or calls prepareInstance on the audio thread
    // (tasks 0008, 0009).
    std::array<moodz::dsp::MultimodeFilter, moodz::dsp::filterSetupCount> filters;
    std::size_t currentFilter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
