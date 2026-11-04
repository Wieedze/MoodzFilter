# 0011 — Crossfade between filter variants

**Phase**: 2
**Layer**: dsp (crossfading filter bank), plugin (processor), tests
**Depends on**: 0010, playbook ADR 0008

## Goal

Changing the Model, a Diode Ladder slope step or the Vintage variant during playback no longer clicks: the outgoing and incoming filter variants run side by side and their outputs are crossfaded over 20 ms (SPEC success criterion 3).

## Expected behaviour (measurable)

Unless stated, the signal is a 200 Hz sine at −12 dBFS, stereo, 48 kHz, blocks of 512, cutoff 5 kHz, resonance 20 %, through `PluginProcessor`. The **click measure** is the largest absolute second difference of the output, `|y[n] − 2 y[n−1] + y[n−2]|`, over the 50 ms after the change; the steady-state value of this sine is about 1.7e-4 (0.25 × (2π × 200 / 48000)²). Every bound comes from a run on the test's own signal, with its command in the row; the red run before the code gives today's click.

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Model change Cytomic SVF Low-pass to Diode Ladder Low-pass at a block boundary | click measure under a bound set from the run | to measure, at least 10 times under the red run | SPEC success criterion 3 |
| Model change Vintage Ladder Low-pass to K35 Low-pass | same | same | same |
| Diode Ladder slope 12 to 18 dB (one step) | same | same | ADR 0008 consequences |
| Vintage variant Runge-Kutta compensated to Huovilainen | same | same | ADR 0008 consequences |
| Slope alternating 14.9 and 15.1 dB on every block, Diode Ladder, 1 s | click measure under the same bound over the whole second | same | task 0010 code review (hysteresis, `FUTURE.md`) |
| Crossfade length | the outgoing variant's contribution is 0 from 20 ms after the change on | ± 1 sample | this task (same ramp time as Gain out, `SmoothedGain::rampSeconds`) |
| Change requested during a running crossfade | the running crossfade finishes, then the latest choice fades in; no sample non-finite | exact | this task |
| No change | output identical to task 0010 (one variant, no crossfade) | max difference under 1e-6 | ADR 0008 |
| OB-Xd Morph or K35 saturation moved (same variant) | no crossfade: the extra value is smoothed by sst-filters as in task 0010 | exact | task 0010 decision notes |
| Model and settings changed on every block through all values, 2 s of noise, resonance 100 % | every sample finite; RTSan build: no report | exact | `.claude/rules/realtime-audio.md` |
| Crossfading bank, block sizes 1, 7, 64, 480, 512, change at sample 4096 | identical output whatever the block size | 1e-6 | `.claude/rules/dsp.md` |
| `check`, `render`, `compare` | render cases unchanged (no change during a render): `compare` green, no `golden:` commit | manifest tolerances | ADR 0003 |

## Out of scope

* Crossfade on an oversampling change (oversampling does not exist yet; its task will reuse this).
* Sample-accurate parameter changes: the choice is still read once per block (`FUTURE.md`).
* Crossfade length as a user parameter.
* Hysteresis on the Diode slope step, unless the alternating-slope row fails with the crossfade alone; then stop and ask.

## Decision notes

* The 17 filter variants and their selection move from the processor into one dsp class, the **crossfading filter bank**: `prepare`, `reset`, `select (FilterChoice)`, `setCutoffHz`, `setResonance`, `process`. It is tested without JUCE. The processor keeps only the snapshot and the call.
* Linear crossfade of the two outputs (equal gain): the two variants filter the same input, so their outputs are correlated and an equal-gain sum keeps the level; an equal-power fade would bump it by up to 3 dB in the middle.
* The incoming variant is reset before it fades in, as in task 0010. During a crossfade both variants run, so the CPU cost of a block doubles for 20 ms; measured at gate 2.
* A change requested during a crossfade is held until the crossfade ends, so at most two variants run at once and the cost stays bounded.
* Search done (`.claude/rules/scope.md` rule 8): `juce::dsp` has no crossfading filter switcher usable in the dsp layer (no JUCE there); sst-filters has none. Own code, about the size of `SmoothedGain`.

## Rules to load

* `.claude/rules/realtime-audio.md`, `dsp.md`, `juce.md`, `testing.md`, `cpp.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [x] Tests written before the code, red then green.
* [x] Debug, Release, RTSan: `ctest` green.
* [x] `check` green; `compare` green against the current references, no `golden:` commit.
* [x] pluginval 10 green.
* [x] `rt-safety-reviewer` passed.
* [x] `GLOSSARY.md` updated (Crossfade, Crossfading filter bank, Click measure).
* [x] **Maxime: the CI installer loads in Live; changing the Model, the Diode slope step and the Vintage variant during playback, and automating them, no longer clicks.** Approved 2026-11-03 at 20:33 with the installer of CI run 41674954975 (commit 15b67b8).
* [x] `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)

2026-11-03, task-verifier: pass (1 round), on 6101e66. Local Debug: `ctest` 75/75 green, `MoodzFilterTools check` all passed, `render` + `compare` against `tests/golden` bit-exact (max error 0, no `golden:` commit). Click measures (`./Builds/PluginTests "[crossfade]" -s | grep click`): 3.6e-4 Cytomic SVF to Diode Ladder, 1.5e-4 Diode slope 12 to 18 dB, 1.6e-4 Vintage Runge-Kutta compensated to Huovilainen, 2.6e-4 slope wobble over 1 s. The bound is 1e-3, more than 90 times under the red run (smallest 0.091); about 1.7e-4 is the sine alone. The crossfade lasts 960 samples at 48 kHz (±1 sample), a choice made during a crossfade starts the sample the running one ends whatever the block size, and the bank output does not depend on the block size (1e-6). The alternating slope passes with the crossfade alone, so no hysteresis. Release, RTSan and pluginval 10 green in CI. Maxime's Live test approved 2026-11-03 20:33. Left in `FUTURE.md`: reversing a crossfade when the choice comes back (A to B to A), one shared sample-rate fallback for every dsp processor. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0011-model-crossfade.md`.
