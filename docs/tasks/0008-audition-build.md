# 0008 — Audition build: filter model, cutoff and resonance in the plugin

**Phase**: 2
**Layer**: plugin, tools
**Depends on**: 0007

## Goal

Maxime chooses the 4 filter models by ear in Ableton Live, on his own music, instead of offline renders (his request, 2026-11-02). The plugin gets three SPEC parameters: **Model** (for now the 7 filter candidates of `dsp/FilterCandidates.h`), **Cutoff** and **Resonance**. The filter runs before Gain out (SPEC signal chain). After his choice, task 0009 keeps 4 models (ADR) and adds Mode.

## Expected behaviour (measurable)

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Parameter IDs | exactly `{ gainOut, filterModel, cutoff, resonance }` | exact | `.claude/rules/juce.md` |
| `filterModel` | choice of the 7 candidates, in the order of `filterCandidates()`, default the first (Cytomic SVF) | exact | SPEC (Model 1 by default) |
| `cutoff` | 20 Hz to 20 kHz, skewed so that the middle of the control is 632 Hz (√(20 × 20 000), a logarithmic feel), default 1 kHz; text `440 Hz`, `1.00 kHz` | exact range, ± 1 Hz centre | SPEC |
| `resonance` | 0 to 100 %, default 20 %; maps to 0..1 for the filter | exact | SPEC |
| New parameters | `juce::ParameterID` version 2 (`gainOut` stays at 1) | exact | `.claude/rules/juce.md` |
| `PluginProcessor`, Cytomic SVF, cutoff 1 kHz, resonance 0, sine 10 kHz then 100 Hz | steady-state gain −34 to −46 dB, then 0 ± 1 dB | as stated | task 0007 measurements (−42.8 dB) |
| Model changed on every block, 2 s of noise | every sample finite; RTSan build: no report | exact | `.claude/rules/realtime-audio.md` |
| State saved before this task (only `gainOut`) | loads; new parameters at their defaults | exact | `.claude/rules/juce.md` (fallback) |
| `check`, `render`, `compare` | gain render cases set cutoff 20 kHz and resonance 0 (filter as open as it gets); references change in a `golden:` commit | manifest tolerances | ADR 0003 |

## Out of scope

* Crossfade when the model changes (a click is possible in this audition build; SPEC success criterion 3, later task).
* Mode (passband), the final list of 4 models, the ADR of the choice (task 0009).
* Smoothing of cutoff and resonance beyond sst-filters' per-packet coefficient smoothing (one-pole, 20 % per 16-sample packet); judged by ear in Live, revisited if zipper noise is heard.
* Drive, oversampling, LFO, mix, gain in.

## Decision notes

* All 7 candidate filters are configured in the processor constructor and prepared in `prepareToPlay`; `processBlock` only selects one, resets it when the selection changes, and processes it. No allocation and no `prepareInstance` on the audio thread.
* No release exists: the Model list will shrink in 0009 without a state migration. Maxime's test sessions with this build may load a different model afterwards.

## Rules to load

* `.claude/rules/juce.md`, `realtime-audio.md`, `testing.md`, `cpp.md`, `workflow.md`

## Definition of done

* [x] Tests written first, seen red (plugin: 6 of 14 failing; model index invariant: did not compile before `toModelIndex`).
* [x] Debug, Release, RTSan: 0 warnings, `ctest` green (42, 42, 43 with the canary), `check` green, `compare` green after the `golden:` commit (ac02387).
* [ ] pluginval 10 green (SUCCESS, Release on f1fe643), `rt-safety-reviewer` passed (no blocking finding; model index bounded in the snapshot and DSP setters marked nonblocking in f1fe643), CI green.
* [x] GLOSSARY checked before the verifier (Filter model, Filter candidate already defined; no new term).
* [ ] **Maxime: the build loads in Live, the 7 models can be switched and heard; golden change accepted.**
* [ ] `task-verifier`: pass.

## End note (filled by the task-verifier)
