# 0008 — Audition build: filter model, cutoff and resonance in the plugin

**Phase**: 2
**Layer**: plugin, tools, dsp (no DSP behaviour change: candidate labels and count in `FilterCandidates.h`, `label` in `FilterSetup`, `MOODZ_NONBLOCKING` on the setters called from `processBlock` after the rt-safety review)
**Depends on**: 0007

## Goal

Maxime chooses the 4 filter models by ear in Ableton Live, on his own music, instead of offline renders (his request, 2026-11-02). The plugin gets three SPEC parameters: **Model** (for now the 7 filter candidates of `dsp/FilterCandidates.h`), **Cutoff** and **Resonance**. The filter runs before Gain out (SPEC signal chain). After his choice, task 0009 keeps 4 models (ADR) and adds Mode.

## Expected behaviour (measurable)

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Parameter IDs | exactly `{ gainOut, filterModel, cutoff, resonance }` | exact | `.claude/rules/juce.md` |
| `filterModel` | choice of the 7 candidates, in the order of `filterCandidates()`, default the first (Cytomic SVF) | exact | SPEC (Model 1 by default) |
| `cutoff` | 20 Hz to 20 kHz, skewed so that the middle of the control is 632 Hz (√(20 × 20 000), a logarithmic feel), default 1 kHz; text `440 Hz`, `1.00 kHz` | exact range, ± 1 Hz centre | SPEC |
| `resonance` | 0 to 100 %, default 20 %; the snapshot maps 0 / 20 / 100 % to 0 / 0.2 / 1 for the filter | ± 1e-6 | SPEC |
| New parameters | `juce::ParameterID` version 2 (`gainOut` stays at 1), checked through `getVersionHint()` | exact | `.claude/rules/juce.md` |
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

* `.claude/rules/juce.md`, `realtime-audio.md`, `dsp.md`, `testing.md`, `cpp.md`, `workflow.md`

## Definition of done

* [x] Tests written first, seen red (plugin: 6 of 14 failing; model index invariant: did not compile before `toModelIndex`).
* [x] Debug, Release, RTSan: 0 warnings, `ctest` green (42, 42, 43 with the canary before b0de2b0; on 898e3f9, Debug 44/44 locally and RTSan 45/45 with the canary in CI run 41606055766), `check` green, `compare` green after the `golden:` commit (ac02387).
* [x] pluginval 10 green (SUCCESS, Release on f1fe643; CI run 41593592904 on 5679f43: Pluginval step success on Windows and Linux), `rt-safety-reviewer` passed (no blocking finding; model index bounded in the snapshot and DSP setters marked nonblocking in f1fe643), CI green (run 41593592904 on 5679f43: Linux, Windows, Linux RealtimeSanitizer success).
* [x] GLOSSARY checked before the verifier (Filter model, Filter candidate already defined; no new term).
* [x] **Maxime: the build loads in Live, the 7 models can be switched and heard; golden change accepted.** 2026-11-03 09:43 CET, installer `MoodzFilter-0.0.1-Windows.exe` from CI run 41593592904 (5679f43): all 7 low pass candidates work in Live. Models kept: Diode Ladder 24 dB, K35, Vintage Ladder, Cytomic SVF, OB-Xd 4-pole 24 dB. That is 5, not the 4 of the SPEC: Maxime chose to keep 5, so task 0009 starts with the SPEC change (grill, ADR, amended SPEC). He also asks for a user-selectable slope on every model that has one (`FUTURE.md`). Golden change of ac02387 accepted after listening to the renders of that run, 2026-11-03 09:43 CET.
* [x] `task-verifier`: pass.

## End note (filled by the task-verifier)

Pass on 2026-11-03, second round. Round 1 failed on four items: no test of the parameter version hints, no test of the full model order, no test of the resonance mapping, and the dsp layer not declared in this file. All four were fixed in b0de2b0 and checked again. On 898e3f9 (Debug, GCC, WSL2): `ctest` 44/44, `check` green, `render` then `compare` green with max error 0 on all 16 files, `git clang-format --diff main` clean, no change in `tests/golden/` beyond the `golden:` commit ac02387. CI run 41606055766 on 898e3f9: Linux, Windows and Linux RealtimeSanitizer green (RTSan `ctest` 45/45 with the canary, model switched on every block without report), pluginval 10 `SUCCESS` on Windows and Linux. Every Expected behaviour row is covered with its tolerance. Maxime kept 5 models, not 4; the SPEC change is the first step of task 0009. Non-blocking points logged in `FUTURE.md`. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0008-audition-build.md`.
