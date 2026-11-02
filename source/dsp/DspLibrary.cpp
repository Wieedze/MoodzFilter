// Anchor translation unit of the dsp static library, so the target exists before its first
// processor. Remove it when the first processor lands (task 0003, smoothed gain).
// Layer rule: nothing under source/dsp/ includes JUCE (.claude/rules/dsp.md); CMake enforces it.
namespace moodz::dsp
{
}
