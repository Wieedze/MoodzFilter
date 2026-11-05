#include "ui/Editor.h"

#include "plugin/PluginProcessor.h"

#include <algorithm>

namespace moodz::ui
{
    int editorHeightFor (int listHeight, int screenHeight) noexcept
    {
        constexpr int hostTitleBar = 80;
        constexpr int juceMinimum = 125; // juce_GenericAudioProcessorEditor.cpp
        const int room = screenHeight > 0 ? std::min (listHeight, screenHeight - hostTitleBar) : listHeight;
        return std::max (juceMinimum, room);
    }

    int tallestScreenHeight()
    {
        float tallest = 0.0f;
        for (const auto& display : juce::Desktop::getInstance().getDisplays().displays)
            tallest = std::max (tallest, display.userBounds.getHeight());
        return juce::roundToInt (tallest);
    }

    juce::Viewport* parameterListViewport (juce::Component& genericEditor)
    {
        for (auto* child : genericEditor.getChildren())
            if (auto* list = dynamic_cast<juce::TreeView*> (child))
                return list->getViewport();
        return nullptr;
    }

    void fitEditorToList (juce::AudioProcessorEditor& genericEditor, int screenHeight)
    {
        const auto* viewport = parameterListViewport (genericEditor);
        // A JUCE update changing this layout must not go unnoticed: the tests fail on it too
        jassert (viewport != nullptr);
        if (viewport != nullptr)
            genericEditor.setSize (genericEditor.getWidth(), editorHeightFor (viewport->getViewedComponent()->getHeight(), screenHeight));
    }
}

// SPEC v1 interface: the JUCE generic editor, one control per parameter, drawn by JUCE.
// Defined in ui/ so that plugin/ does not depend on the editor.
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    // JUCE opens it at 400 pixels at most, whatever the number of parameters (task 0016)
    auto editor = std::make_unique<juce::GenericAudioProcessorEditor> (*this);
    moodz::ui::fitEditorToList (*editor, moodz::ui::tallestScreenHeight());
    return editor.release();
}
