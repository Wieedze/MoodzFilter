#include "ui/Editor.h"

#include "plugin/PluginProcessor.h"

#include <algorithm>

namespace moodz::ui
{
    int editorHeightFor (int listHeight, int availableHeight) noexcept
    {
        return availableHeight > 0 ? std::min (listHeight, availableHeight) : listHeight;
    }

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
}

// SPEC v1 interface: the JUCE generic editor, one control per parameter, drawn by JUCE.
// Defined in ui/ so that plugin/ does not depend on the editor.
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    auto editor = std::make_unique<juce::GenericAudioProcessorEditor> (*this);

    // JUCE opens it at 400 pixels at most, whatever the number of parameters (task 0016)
    if (const auto* list = moodz::ui::findList (*editor))
        if (const auto* viewport = list->getViewport(); viewport != nullptr && viewport->getViewedComponent() != nullptr)
        {
            const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            const int available = display != nullptr ? juce::roundToInt (display->userBounds.getHeight()) : 0;
            editor->setSize (editor->getWidth(), moodz::ui::editorHeightFor (viewport->getViewedComponent()->getHeight(), available));
        }

    return editor.release();
}
