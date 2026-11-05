#include "plugin/PluginProcessor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
    // The filter banks configure every filter variant in their constructors, off the audio thread.
    // One latency at every Oversampling setting, known before the host prepares us (ADR 0009).
    setLatencySamples (moodz::dsp::OversampledFilterChain::latencySamples);
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    // Steinberg's VST3 validator fails plugins whose single default program has no name
    return "Default";
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    bypassDelay.prepare (moodz::dsp::OversampledFilterChain::latencySamples);
    gainOut.prepare (sampleRate, samplesPerBlock);
    // Start at the current parameter value, without a ramp from the default
    const auto params = snapshotter.snapshot();
    gainOut.setTargetDb (params.gainOutDb);
    gainOut.reset();

    // Start on the current choice and factor, without a crossfade from the default
    filters.setCutoffHz (params.cutoffHz);
    filters.setResonance (params.resonance);
    filters.setFactorIndex (params.oversampling);
    filters.setDriveDb (params.driveDb);
    filters.setDriveCurveIndex (params.driveCurve);
    filters.setLfoShapeIndex (params.lfoShape);
    filters.setLfoRateIndex (params.lfoRate);
    filters.setLfoDepthOctaves (params.lfoDepthOctaves);
    filters.select (moodz::dsp::chooseFilter (params.filterModel, params.slopeDb, params.vintageVariant, params.k35Saturation));
    filters.prepare (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages) noexcept MOODZ_NONBLOCKING
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    const auto params = snapshotter.snapshot();
    gainOut.setTargetDb (params.gainOutDb);

    // The Model entry and its settings select a filter variant (ADR 0008); a new variant
    // crossfades in over 20 ms (task 0011). chooseFilter and the bank both bound the index.
    filters.select (moodz::dsp::chooseFilter (params.filterModel, params.slopeDb, params.vintageVariant, params.k35Saturation));
    filters.setFactorIndex (params.oversampling);
    filters.setCutoffHz (params.cutoffHz);
    filters.setResonance (params.resonance);
    filters.setDriveDb (params.driveDb);
    filters.setDriveCurveIndex (params.driveCurve);
    filters.setLfoShapeIndex (params.lfoShape);
    filters.setLfoRateIndex (params.lfoRate);
    filters.setLfoDepthOctaves (params.lfoDepthOctaves);

    // While the host plays, the song position sets the LFO; otherwise it runs on, at the last tempo (task 0015)
    if (const auto* host = getPlayHead())
        if (const auto position = host->getPosition())
        {
            if (const auto bpm = position->getBpm())
                filters.setTempoBpm (*bpm);
            if (position->getIsPlaying())
                if (const auto quarterNotes = position->getPpqPosition())
                    filters.syncLfoToPosition (*quarterNotes);
        }

    // Mono or stereo only (isBusesLayoutSupported), so two spans are enough
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = std::min ({ totalNumOutputChannels, buffer.getNumChannels(), 2 });
    std::array<std::span<float>, 2> channels {};
    for (int channel = 0; channel < numChannels; ++channel)
        channels[static_cast<size_t> (channel)] = { buffer.getWritePointer (channel), static_cast<size_t> (numSamples) };

    // SPEC signal chain: drive and filter, then gain out
    const std::span<const std::span<float>> active { channels.data(), static_cast<size_t> (numChannels) };
    filters.process (active, numSamples);
    gainOut.process (active, numSamples);
}

void PluginProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages) noexcept MOODZ_NONBLOCKING
{
    juce::ignoreUnused (midiMessages);

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = std::min ({ getTotalNumOutputChannels(), buffer.getNumChannels(), 2 });
    std::array<std::span<float>, 2> channels {};
    for (int channel = 0; channel < numChannels; ++channel)
        channels[static_cast<size_t> (channel)] = { buffer.getWritePointer (channel), static_cast<size_t> (numSamples) };
    bypassDelay.process ({ channels.data(), static_cast<size_t> (numChannels) }, numSamples);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

// createEditor() is defined in ui/Editor.cpp: the plugin layer does not know the editor

//==============================================================================
// What the state holds: every parameter of the APVTS. Not in it: nothing yet (no bypass,
// no session settings). A state written before stateVersion existed is read as version 1.
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    std::unique_ptr<juce::XmlElement> xml (copy.createXml());
    xml->setAttribute ("stateVersion", stateVersion);
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName (state.state.getType()))
        return;

    // Version 1 is the only one so far; a missing attribute means version 1 too
    const auto version = xml->getIntAttribute ("stateVersion", 1);
    juce::ignoreUnused (version);

    state.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
