# 0001 — Initialise the repo from Pamplejuce

**Phase**: 1
**Layer**: build / docs
**Depends on**: none

## Goal

The Pamplejuce template becomes MoodzFilter: its identity, VST3 only, JUCE pinned to a tag, the playbook layer in place, a green local build and test run, and a CI that no longer depends on IPP, macOS or signing secrets.

## Expected behaviour (measurable)

| Check | Expected result |
| --- | --- |
| `PluginProcessor::getName()` | `"MoodzFilter"` (test `Plugin instance / name`) |
| `acceptsMidi()`, `producesMidi()`, `isMidiEffect()` | all `false` (test `Plugin instance / is an effect without MIDI`) |
| Build artefacts | only `VST3/MoodzFilter.vst3` under `Builds/MoodzFilter_artefacts/<config>/` |
| `JUCE` submodule | at tag `9.0.5`, no `branch =` line in `.gitmodules` |
| Debug and Release builds (Linux, clang) | zero warnings |
| pluginval 1.0.4, strictness 10, Release VST3 | `SUCCESS` |

## Out of scope

* Separating `dsp`, `plugin`, `ui`, `tools` targets (task 0002).
* Any parameter, any DSP, any editor change beyond guarding the inspector.
* RealtimeSanitizer and audio-regression CI jobs (task "Green CI").
* Signing setup itself: only made conditional.

## Rules to load

* `.claude/rules/build.md`, `.claude/rules/scope.md`, `.claude/rules/workflow.md`

## Skills

* none

## Definition of done

* [x] Identity from plugin ADR 0002 in `CMakeLists.txt` (`MoodzFilter`, `Radikal Moodz`, `Rdkm`, `Mzf1`, `VST3`, no MIDI).
* [x] JUCE pinned to `9.0.5`; `cmake/` submodule updated to its latest `main`; CLAP submodule removed.
* [x] IPP removed (CMake, CI, tests); `include(Sanitizers)` after `project()`; melatonin_inspector only in Debug.
* [x] CI: Linux and Windows only, no IPP, no macOS, Windows signing only when the Azure secrets exist; `nightly.yml` and `linux-ci-image.yml` removed.
* [x] Installer: VST3 only, licence page shows the AGPLv3.
* [x] `LICENSE` is AGPLv3 (plugin ADR 0001); the Pamplejuce MIT notice kept in `THIRD_PARTY_NOTICES.txt`.
* [x] Playbook layer copied: `.claude/rules`, `.claude/agents`, templates, `GLOSSARY.md`, `docs/tasks/TEMPLATE.md`; router `CLAUDE.md` replaces the template's.
* [x] `cmake --build` with no warning, `ctest` green, pluginval 10 green (Linux).
* [x] CI green on GitHub (Linux, Windows): run 41461476342.
* [x] `task-verifier`: pass.

## End note (filled by the task-verifier)

2026-11-02, task-verifier: **pass** (1 round). Every row of "Expected behaviour" checked: name and no-MIDI tests green, only `VST3/MoodzFilter.vst3` built, JUCE at tag `9.0.5` with no `branch =`, clean Release build with 0 warnings, Debug rebuild with no warning, pluginval 1.0.4 strictness 10 `SUCCESS` on the Release VST3, CI run 41461476342 green on Linux and Windows for `aaab18e`. The CI platform list deviates from `.claude/rules/build.md` (no macOS) but follows plugin ADR 0002; follow-up to align the rule is in the post-mortem. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0001-init-from-pamplejuce.md`.
