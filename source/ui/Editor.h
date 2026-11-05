#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace moodz::ui
{
    // The height that shows a list of the given height, on a screen with that usable height: room is
    // left for the title bar the host adds, and JUCE's own minimum is kept. At or below 0, no screen is
    // known and the list alone decides (task 0016).
    int editorHeightFor (int listHeight, int screenHeight) noexcept;

    // The tallest usable height among the displays: the host chooses the screen, and we are not told
    // which when the editor is created. 0 without a display.
    int tallestScreenHeight();

    // The list JUCE builds in its generic editor: its only TreeView, a direct child.
    juce::Viewport* parameterListViewport (juce::Component& genericEditor);

    // Sets the editor's height from its parameter list.
    void fitEditorToList (juce::AudioProcessorEditor& genericEditor, int screenHeight);
}
