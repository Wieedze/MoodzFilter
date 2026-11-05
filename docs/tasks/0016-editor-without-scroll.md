# 0016 — Show every parameter without scrolling

**Phase**: 2
**Layer**: ui, tests
**Depends on**: 0015

## Goal

The editor opens tall enough to show every parameter, so nothing needs scrolling on a normal screen. It stays the JUCE generic editor and stays resizable.

Requested by Maxime after the task 0015 Live test, 2026-11-05.

## Expected behaviour (measurable)

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| The editor at its opening size | the list's vertical scroll bar is not visible, the whole list fits | exact | Goal |
| A parameter added later | the height follows the number of parameters, no constant to update | exact | Mix and Gain in are still to come |
| A screen shorter than the list | the height is held to the screen's usable height; the scroll bar comes back only there | exact | The window must stay reachable on a small screen |
| The editor class | still `juce::GenericAudioProcessorEditor`, still resizable | exact | SPEC interface |
| Audio | no change: `compare` green, no `golden:` commit | manifest tolerances | The editor contains no audio logic |

## Out of scope

* A custom layout, groups, two columns, or any drawing: SPEC Out of v1, item 5.
* An LFO matrix: for after v1, a SPEC change (`FUTURE.md`).
* Remembering the window size between sessions.

## Decision notes

* JUCE sizes the generic editor to at most 400 pixels (`juce_GenericAudioProcessorEditor.cpp`, `jlimit (125, 400, ...)`) with 40 pixels per parameter, so 10 parameters fit and the 13 of today do not. After construction, `createEditor` sets the height from the list's own height: the number of parameters times JUCE's item height.
* The item height is read from the `TreeView` JUCE builds, not repeated here; a test fails if a JUCE update changes the layout.
* The screen limit uses the primary display's usable area. With no display (headless tests) there is no limit.
* **Search done** (`.claude/rules/scope.md` rule 8): JUCE has no option for this height; a patch to JUCE is not worth it for one `setSize`.

## Rules to load

* `.claude/rules/cpp.md`, `juce.md`, `ui.md`, `testing.md`, `workflow.md`

## Definition of done

* [ ] Test written before the code, seen red.
* [ ] Debug, Release: `ctest` green; `check` and `compare` green, no golden change.
* [ ] pluginval 10 green (it opens the editor).
* [ ] Mutations on the new tests go red.
* [ ] **Maxime: the CI installer in Live, at his screen scale: every parameter visible without scrolling; the window resizes; nothing changes in the sound.** This is the check `.claude/rules/ui.md` asks a screenshot for, if `tools snapshot` cannot run here.
* [ ] `code-review`, `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
