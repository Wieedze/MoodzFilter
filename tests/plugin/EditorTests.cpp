#include "plugin/PluginProcessor.h"
#include "ui/Editor.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace
{
    juce::TreeView* findList (juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* list = dynamic_cast<juce::TreeView*> (child))
                return list;
            if (auto* inner = findList (*child))
                return inner;
        }
        return nullptr;
    }
}

TEST_CASE ("The editor shows every parameter without scrolling", "[plugin][ui]")
{
    // Task 0016, Maxime after the task 0015 Live test. Asked of the list JUCE builds: its content,
    // at least one row of JUCE's 40 pixels per parameter, fits its viewport.
    PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->resized();
    auto* list = findList (*editor);
    REQUIRE (list != nullptr);
    auto* viewport = list->getViewport();
    REQUIRE (viewport != nullptr);
    const auto rows = processor.getParameters().size();
    REQUIRE (rows > 10); // more than the 400 pixels JUCE opens at
    CHECK (viewport->getViewedComponent()->getHeight() >= rows * 40);
    CHECK (viewport->getViewHeight() >= viewport->getViewedComponent()->getHeight());
    CHECK (!viewport->getVerticalScrollBar().isVisible());
}

TEST_CASE ("The editor stays the generic one, resizable", "[plugin][ui]")
{
    // SPEC interface. The width stays the one JUCE chose.
    PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::GenericAudioProcessorEditor plain (processor);
    CHECK (editor->getWidth() == plain.getWidth());
    CHECK (editor->isResizable());
}

TEST_CASE ("The editor's height is held to the screen", "[plugin][ui]")
{
    // Task 0016: the window must stay reachable on a short screen. No display known: no limit.
    using moodz::ui::editorHeightFor;
    CHECK (editorHeightFor (520, 1080) == 520);
    CHECK (editorHeightFor (520, 520) == 520);
    CHECK (editorHeightFor (520, 400) == 400);
    CHECK (editorHeightFor (520, 0) == 520);
    CHECK (editorHeightFor (520, -1) == 520);
}
