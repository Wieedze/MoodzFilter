#include "plugin/PluginProcessor.h"
#include "ui/Editor.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using moodz::ui::editorHeightFor;
using moodz::ui::fitEditorToList;
using moodz::ui::parameterListViewport;
using moodz::ui::tallestScreenHeight;

TEST_CASE ("With room on the screen the editor shows every parameter", "[plugin][ui]")
{
    // Task 0016, Maxime after the task 0015 Live test. Asked of the list JUCE builds, on a generic editor
    // sized for a tall screen, so the test does not depend on the machine's own screen: its content, at
    // least one row of JUCE's 40 pixels per parameter, fits its viewport without a scroll bar.
    PluginProcessor processor;
    juce::GenericAudioProcessorEditor editor (processor);
    fitEditorToList (editor, 4000);
    auto* viewport = parameterListViewport (editor);
    REQUIRE (viewport != nullptr);
    const auto rows = processor.getParameters().size();
    REQUIRE (rows > 10); // more than the 400 pixels JUCE opens at
    CHECK (viewport->getViewedComponent()->getHeight() >= rows * 40);
    CHECK (viewport->getViewHeight() >= viewport->getViewedComponent()->getHeight());
    CHECK (!viewport->getVerticalScrollBar().isVisible());
}

TEST_CASE ("On a short screen the editor is held to it and scrolls", "[plugin][ui]")
{
    // Task 0016: the window must stay reachable, so the scroll bar comes back only there.
    PluginProcessor processor;
    juce::GenericAudioProcessorEditor editor (processor);
    fitEditorToList (editor, 400);
    CHECK (editor.getHeight() == editorHeightFor (parameterListViewport (editor)->getViewedComponent()->getHeight(), 400));
    CHECK (editor.getHeight() < 400);
    CHECK (parameterListViewport (editor)->getVerticalScrollBar().isVisible());
}

TEST_CASE ("The plugin opens its editor fitted to the screen it knows", "[plugin][ui]")
{
    // createEditor does the fitting, on the tallest screen, and the editor stays the generic one, resizable,
    // with the width JUCE chose. SPEC interface.
    PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::GenericAudioProcessorEditor plain (processor);
    CHECK (dynamic_cast<juce::GenericAudioProcessorEditor*> (editor.get()) != nullptr);
    CHECK (editor->isResizable());
    CHECK (editor->getWidth() == plain.getWidth());
    const int list = parameterListViewport (*editor)->getViewedComponent()->getHeight();
    CHECK (editor->getHeight() == editorHeightFor (list, tallestScreenHeight()));
}

TEST_CASE ("The editor's height keeps a margin and a minimum", "[plugin][ui]")
{
    // Task 0016, code review. 80 pixels for the host's title bar; 125 is JUCE's own minimum
    // (juce_GenericAudioProcessorEditor.cpp). No screen known: the list decides.
    CHECK (editorHeightFor (520, 1080) == 520);
    CHECK (editorHeightFor (520, 600) == 520);
    CHECK (editorHeightFor (520, 599) == 519);
    CHECK (editorHeightFor (520, 400) == 320);
    CHECK (editorHeightFor (520, 100) == 125);
    CHECK (editorHeightFor (520, 0) == 520);
    CHECK (editorHeightFor (520, -1) == 520);
    CHECK (editorHeightFor (60, 1080) == 125);
}
