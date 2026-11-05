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
}

TEST_CASE ("The tallest of several screens decides", "[plugin][ui]")
{
    // Task 0016 verifier: the machine's own screens are too tall to show the limit, so the test replaces
    // JUCE's public list of displays. Screens of 400 and 500 usable pixels, in both orders: 500 - 80 for the
    // host's title bar is 420, under the list's height, so the scroll bar is back.
    auto& displays = const_cast<juce::Displays&> (juce::Desktop::getInstance().getDisplays()).displays;
    const auto saved = displays;
    for (const bool tallestFirst : { true, false })
    {
        INFO ("tallest first " << tallestFirst);
        juce::Displays::Display shorter, taller;
        shorter.userBounds = { 0.0f, 0.0f, 800.0f, 400.0f };
        taller.userBounds = { 0.0f, 0.0f, 800.0f, 500.0f };
        displays.clearQuick();
        displays.add (tallestFirst ? taller : shorter);
        displays.add (tallestFirst ? shorter : taller);
        CHECK (tallestScreenHeight() == 500);
        PluginProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        CHECK (editor->getHeight() == 420);
        CHECK (parameterListViewport (*editor)->getVerticalScrollBar().isVisible());
    }
    displays = saved;
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
