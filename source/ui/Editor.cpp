#include "plugin/PluginProcessor.h"

// SPEC v1 interface: the JUCE generic editor, one control per parameter, drawn by JUCE.
// Defined in ui/ so that plugin/ does not depend on the editor.
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
