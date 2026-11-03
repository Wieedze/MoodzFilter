# 0009 — Freeze the five filter models and add Mode

**Phase**: 2
**Layer**: dsp (the v1 model list replaces the candidate list), plugin (parameters, processor), tests
**Depends on**: 0008, playbook ADR 0006

## Goal

The Model parameter offers the 5 models Maxime chose in task 0008, in the frozen order of ADR 0006, and a new Mode parameter picks the passband: the Cytomic SVF plays low-pass, high-pass, band-pass and notch, the K35 low-pass and high-pass, and every passband a model lacks plays low-pass.

## Expected behaviour (measurable)

Unless stated, the signal is a sine at −12 dBFS, 48 kHz, blocks of 512, cutoff 1 kHz, resonance 0, through `PluginProcessor`; the gain is the steady-state RMS ratio after 0.5 s. A bound marked "measured" is set during the task from a run on the test's own signal, with the command written in this row before the verifier runs (no bound taken from a reviewer or from theory).

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Parameter IDs | exactly `{ gainOut, filterModel, cutoff, resonance, filterMode }` | exact | `.claude/rules/juce.md` |
| `filterModel` | choices `Cytomic SVF`, `Vintage Ladder`, `OB-Xd 4-pole`, `Diode Ladder`, `K35`, in this order; default index 0 | exact | ADR 0006 |
| `filterMode` | choices `Low-pass`, `High-pass`, `Band-pass`, `Notch`, in this order; default `Low-pass` | exact | SPEC, ADR 0006 |
| Version hints | `gainOut` 1; `filterModel`, `cutoff`, `resonance` 2; `filterMode` 3 | exact | `.claude/rules/juce.md` |
| Model and mode indices from the raw value | NaN, ±∞, −1, 99 give a model index in 0..4 and a mode index in 0..3 | exact | task 0008 invariant, list now 5 long |
| Cytomic SVF, Low-pass, 10 kHz then 100 Hz | −34 to −46 dB, then 0 ± 1 dB | as stated | task 0008 test (unchanged) |
| Cytomic SVF, High-pass, 100 Hz then 10 kHz | −34 to −46 dB, then 0 ± 1 dB | as stated | task 0007 measurement (HP 100 Hz −40.1 dB) |
| Cytomic SVF, Band-pass, 100 Hz, 1 kHz, 10 kHz | 1 kHz more than 12 dB over 100 Hz and over 10 kHz | measured 14.1 and 15.4 dB: `./Builds/PluginTests "Cytomic SVF band pass and notch" -s \| grep -E "band pass\|notch 1"` | SVF band-pass |
| Cytomic SVF, Notch, 100 Hz, 1 kHz, 10 kHz | 1 kHz more than 60 dB under 100 Hz and under 10 kHz | measured −122.5 dB at 1 kHz, −0.18 and −0.13 dB either side: same command | SVF notch |
| K35, High-pass, 100 Hz then 10 kHz | 10 kHz more than 16 dB over 100 Hz | measured 20.1 dB: `./Builds/PluginTests "K35 high pass" -s \| grep "K35 high pass 100"` | K35 high-pass |
| Fallback: Vintage Ladder, OB-Xd 4-pole, Diode Ladder with High-pass, Band-pass, Notch; K35 with Band-pass, Notch; 1 s of white noise (`juce::Random` seed 7, peak 0.25) | output equal to the same model in Low-pass | max abs difference ≤ 1e-6 | ADR 0006 |
| Model and Mode changed on every block, all 20 pairs, 2 s of noise | every sample finite; RTSan build: no report | exact | `.claude/rules/realtime-audio.md` |
| State saved by task 0008 (`gainOut`, `filterModel`, `cutoff`, `resonance`) | loads; `filterMode` at `Low-pass`, the other values kept | exact | `.claude/rules/juce.md` (fallback) |
| Every model × supported passband (9 filter setups), noise, at 44.1, 48, 96, 192 kHz, cutoff 20 Hz, 1 kHz, 20 kHz, resonance 0, 0.5, 1 | finite output, peak under the task 0007 bound of 4 | measured largest peak 2.59: `./Builds/DspTests "Every filter setup stays finite at the bounds" -s \| grep "largest peak"` | task 0007 test, extended |
| `check`, `render`, `compare` | render cases unchanged (default model and Mode, filter wide open): `compare` green against the current references, no `golden:` commit | manifest tolerances | ADR 0003 |

## Out of scope

* The per-model settings of ADR 0006 (slope, Morph, K35 saturation, Vintage Ladder sub-model): task 0010. The 5 models keep the task 0008 configurations (Vintage Ladder Runge-Kutta compensated, OB-Xd 4-pole 24 dB, Diode Ladder 24 dB, K35 Mild).
* Showing the fallback in the Mode text (for example `Notch (plays low-pass)`): the choice texts are fixed; a display that follows the model is a separate question, logged in `FUTURE.md`.
* The Cytomic SVF passbands beyond the SPEC's four (peak, all-pass, shelves, bell).
* Crossfade on model or Mode change (a click is possible; SPEC success criterion 3, later task).
* Model blending, random curve on the cutoff (ADR 0006, `FUTURE.md`).
* Drive, oversampling, LFO, mix, gain in.
* A state migration for Maxime's 0008 test sessions: the Model list shrinks from 7 to 5 with no release, so a saved index may now point to another model (task 0008 decision notes).

## Decision notes

* `dsp/FilterModels.h` replaces `dsp/FilterCandidates.h`: the v1 list of ADR 0006, each model with the passbands it supports. "Filter candidate" leaves the code; `GLOSSARY.md` is updated in this task.
* One `MultimodeFilter` per (model, supported passband) pair, configured in the constructor and prepared in `prepareToPlay`, as in task 0008: 4 + 1 + 1 + 1 + 2 = 9 instances. `processBlock` maps (Model, Mode) to an instance, with the fallback applied in that mapping, and resets the instance when the pair changes. No `configure` or `prepareInstance` on the audio thread.
* The fallback is a table next to the model list (`dsp::filterSetupIndex`), not a DSP behaviour; it is tested through `PluginProcessor`.

## Rules to load

* `.claude/rules/juce.md`, `realtime-audio.md`, `dsp.md`, `testing.md`, `cpp.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [ ] Tests written before the code, red then green.
* [ ] Debug, Release, RTSan (CI): no new warning, `ctest` green.
* [ ] `check` green; `compare` green against the current references, no `golden:` commit.
* [ ] pluginval 10 green (CI).
* [ ] `rt-safety-reviewer` passed.
* [ ] `GLOSSARY.md` updated (Filter candidate, Filter model list, Mode fallback).
* [ ] **Maxime: the CI installer loads in Live; Model shows the 5 models in order; Mode changes the sound on Cytomic SVF (4 passbands) and K35 (low-pass, high-pass); on the 3 others every Mode sounds low-pass.**
* [ ] `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
