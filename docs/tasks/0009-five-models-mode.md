# 0009 — Freeze the filter list: five models with their passbands

**Phase**: 2
**Layer**: dsp (the v1 filter list replaces the candidate list), plugin (parameters, processor), tests
**Depends on**: 0008, playbook ADRs 0006 and 0007

## Goal

The Model parameter offers the 9 entries of ADR 0007, in their frozen order: each of the 5 models Maxime chose in task 0008 with each passband it really has (Cytomic SVF low-pass, high-pass, band-pass, notch; Vintage Ladder, OB-Xd 4-pole and Diode Ladder low-pass; K35 low-pass and high-pass). No entry plays something other than its name.

## Rescoped during the task

The first version followed ADR 0006: 5 models and a fixed Mode parameter, with a passband the model lacks playing low-pass (commit `8c85445`). Maxime checked it in Live on 2026-11-03 at 14:10 CET (CI run 41636044276): conforming. He then asked that a model show only the passbands it has, which a VST3 parameter cannot do with a list that follows another parameter. He chose to merge model and passband in the Model list (ADR 0007, same day), so `filterMode` is removed before any release.

## Expected behaviour (measurable)

Unless stated, the signal is a sine at −12 dBFS, 48 kHz, blocks of 512, cutoff 1 kHz, resonance 0, through `PluginProcessor`; the gain is the steady-state RMS ratio after 0.5 s. Every bound comes from a run on the test's own signal, with its command in the row.

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Parameter IDs | exactly `{ gainOut, filterModel, cutoff, resonance }` | exact | `.claude/rules/juce.md`, ADR 0007 |
| `filterModel` | choices `Cytomic SVF Low-pass`, `Cytomic SVF High-pass`, `Cytomic SVF Band-pass`, `Cytomic SVF Notch`, `Vintage Ladder Low-pass`, `OB-Xd 4-pole Low-pass`, `Diode Ladder Low-pass`, `K35 Low-pass`, `K35 High-pass`, in this order; default index 0 | exact | ADR 0007 |
| Version hints | `gainOut` 1; `filterModel`, `cutoff`, `resonance` 2 | exact | `.claude/rules/juce.md` |
| Model index from the raw value | NaN, ±∞, −1, 99 give an index in 0..8 | exact | task 0008 invariant, list now 9 long |
| Cytomic SVF Low-pass, 10 kHz then 100 Hz | −34 to −46 dB, then 0 ± 1 dB | as stated | task 0008 test (unchanged) |
| Cytomic SVF High-pass, 100 Hz then 10 kHz | −34 to −46 dB, then 0 ± 1 dB | measured −40.1 dB at 100 Hz: `./Builds/PluginTests "Cytomic SVF high pass" -s \| grep "100 Hz gain"` | task 0007 measurement |
| Cytomic SVF Band-pass, 100 Hz, 1 kHz, 10 kHz | 1 kHz more than 12 dB over 100 Hz and over 10 kHz | measured 14.1 and 15.4 dB: `./Builds/PluginTests "Cytomic SVF band pass and notch" -s \| grep -E "band pass\|notch 1"` | SVF band-pass |
| Cytomic SVF Notch, 100 Hz, 1 kHz, 10 kHz | 1 kHz more than 60 dB under 100 Hz and under 10 kHz | measured −122.5 dB at 1 kHz, −0.18 and −0.13 dB either side: same command | SVF notch |
| K35 High-pass, 100 Hz then 10 kHz | 10 kHz more than 16 dB over 100 Hz | measured 20.1 dB: `./Builds/PluginTests "K35 high pass" -s \| grep "K35 high pass 100"` | K35 high-pass |
| Model changed on every block through the 9 entries, 2 s of noise, resonance 100 % | every sample finite; RTSan build: no report | exact | `.claude/rules/realtime-audio.md` |
| State saved by task 0008 (`gainOut`, `filterModel`, `cutoff`, `resonance`) | loads with every value kept | exact | `.claude/rules/juce.md` |
| State saved by the first version of this task (`8c85445`, with `filterMode`) | loads; `filterMode` ignored, the other values kept | exact | `.claude/rules/juce.md` (fallback) |
| Every entry (9 filter setups), noise, at 44.1, 48, 96, 192 kHz, cutoff 20 Hz, 1 kHz, 20 kHz, resonance 0, 0.5, 1 | finite output, peak under the task 0007 bound of 4 | measured largest peak 2.59: `./Builds/DspTests "Every filter setup stays finite at the bounds" -s \| grep "largest peak"` | task 0007 test, extended |
| `check`, `render`, `compare` | render cases unchanged (default entry, filter wide open): `compare` green against the current references, no `golden:` commit | manifest tolerances | ADR 0003 |

## Out of scope

* The per-model settings of ADR 0006 (slope, Morph, K35 saturation, Vintage Ladder sub-model): task 0010, which also decides whether they join this list (ADR 0007). The models keep the task 0008 configurations (Vintage Ladder Runge-Kutta compensated, OB-Xd 4-pole 24 dB, Diode Ladder 24 dB, K35 Mild).
* The Cytomic SVF passbands beyond low, high, band and notch (peak, all-pass, shelves, bell).
* Crossfade on a Model change (a click is possible; SPEC success criterion 3, later task).
* Model blending, random curve on the cutoff (ADR 0006, `FUTURE.md`).
* Drive, oversampling, LFO, mix, gain in.
* A state migration for Maxime's test sessions: no release exists, so a saved index may now point to another entry (task 0008 decision notes).

## Decision notes

* `dsp/FilterModels.h` replaces `dsp/FilterCandidates.h`: `filterSetups()` is the v1 list, in the order of the Model parameter, and its labels are the Model choices. "Filter candidate" leaves the code; `GLOSSARY.md` is updated in this task.
* One `MultimodeFilter` per entry, configured in the constructor and prepared in `prepareToPlay`, as in task 0008: 9 instances. `processBlock` selects the entry and resets it when the selection changes. No `configure` or `prepareInstance` on the audio thread.

## Rules to load

* `.claude/rules/juce.md`, `realtime-audio.md`, `dsp.md`, `testing.md`, `cpp.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [x] Tests written before the code, red then green (first version 8c85445: compile red, then behaviour red with the mapping broken on purpose, 3 of 12 filter cases failing; rescope 3232c5d: 6 of 22 plugin cases red before the code).
* [x] Debug, Release, RTSan: `ctest` green. Locally on f9cb1d7 (Debug, GCC, WSL2): 50/50. CI run 41643513241 on f9cb1d7: Linux and Windows (Release) and Linux RealtimeSanitizer (Debug, clang 20) green.
* [x] `check` green (`check: all passed`); `compare` green against the current references (`compare: all files within tolerance`, max error 0), no `golden:` commit, `tests/golden/` untouched.
* [x] pluginval 10 green (CI run 41643513241: Pluginval step success on Windows and Linux).
* [x] `rt-safety-reviewer` passed (first version 8c85445: no finding; rescoped 3232c5d: no real-time violation, 1 low point, the local index bound removed with Mode, restored in the next commit).
* [x] `GLOSSARY.md` updated (Filter candidate, Filter list, Filter setup, Passband).
* [x] Maxime, first version (`8c85445`, 5 models and Mode): conforming in Live, 2026-11-03 14:10 CET.
* [x] **Maxime: the CI installer loads in Live; Model shows the 9 entries in order, each one sounds like its name.** 2026-11-03 14:29 CET, installer `MoodzFilter-0.0.1-Windows.exe` from CI run 41643513241 (f9cb1d7): everything works.
* [x] `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)

Pass on 2026-11-03, first round, judged against the rescoped task and ADR 0007. On c086c78 (Debug, GCC, WSL2): `ctest` 50/50, `check` green, `render` then `compare` green with max error 0, `git clang-format --diff main` clean, no change in `tests/golden/`. Every measurement command of the Expected behaviour table reprinted the figures of its row (HP 100 Hz −40.12 dB; band pass 14.14 and 15.41 dB; notch −122.5 dB at 1 kHz; K35 HP 20.10 dB; largest peak 2.59). CI run 41643513241 on f9cb1d7: Linux and Windows (Release, pluginval 10 `SUCCESS`) and Linux RealtimeSanitizer green. Every row is covered by a test with its tolerance; no parameter added, `filterMode` removed before any release. Non-blocking points: the dsp tests' `candidate` variables and an ADR citation, fixed at close; a removed `filterMode` value lingering in loaded states, logged in `FUTURE.md`; the state tests build their XML by hand, acceptable until the first release puts state files in `tests/plugin/states/`. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0009-five-models-mode.md`.
