#pragma once

namespace moodz::ui
{
    // The height that shows a list of the given height, held to the screen's usable height when one
    // is known: at or below 0, there is no display to fit (task 0016).
    int editorHeightFor (int listHeight, int availableHeight) noexcept;
}
