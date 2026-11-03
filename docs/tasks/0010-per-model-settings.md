# 0010 — Per-model settings: Slope, K35 saturation, Vintage variant

**Phase**: 2
**Layer**: dsp (filter variants, extra coefficient), plugin (parameters, processor), tests
**Depends on**: 0009, playbook ADRs 0006, 0007, 0008

## Goal

Three new parameters give each model its own character, as ADR 0008 decided: **Slope** (6 to 24 dB per octave, continuous on the OB-Xd 4-pole, nearest 6 dB step on the Diode Ladder), **K35 saturation** (0 to 100 %, K35 Low-pass and High-pass), **Vintage variant** (the 6 Vintage Ladder sub-models). Their defaults give back the task 0009 sound. The Model list does not change.

## Expected behaviour (measurable)

Unless stated, the signal is a sine at −12 dBFS, 48 kHz, blocks of 512, cutoff 1 kHz, resonance 0, through `PluginProcessor`; the gain is the steady-state RMS ratio after 0.5 s. Every bound comes from a run on the test's own signal, with its command in the row.

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Parameter IDs | exactly `{ gainOut, filterModel, cutoff, resonance, slope, k35Saturation, vintageVariant }` | exact | `.claude/rules/juce.md`, ADR 0008 |
| Version hints | `gainOut` 1; `filterModel`, `cutoff`, `resonance` 2; `slope`, `k35Saturation`, `vintageVariant` 3 | exact | `.claude/rules/juce.md` |
| `slope` | 6 to 24 dB per octave, default 24, display `"24 dB/oct"` | exact | ADR 0008 |
| `k35Saturation` | 0 to 100 %, default 25 % | exact | ADR 0008 (25 % = sst-filters Mild) |
| `vintageVariant` | choices `Runge-Kutta`, `Runge-Kutta compensated`, `Huovilainen`, `Huovilainen compensated`, `Huovilainen 2010`, `Huovilainen 2010 compensated`, in this order; default `Runge-Kutta compensated` | exact | ADR 0008, sst-filters `FilterSubModel` |
| Defaults against task 0009, every Model entry, noise, 1 s | output of the default settings equals the task 0009 configuration of that entry (Vintage RK compensated, OB-Xd 24 dB, Diode 24 dB, K35 Mild) | largest difference under 1e-6; measured 0 on all 9 entries, OB-Xd Morph at 1 included: `./Builds/DspTests "The default settings give back the task 0009 sound" -s \| grep "largest difference"` | ADR 0008 consequences |
| Diode Ladder, slope 6, 12, 18, 24 dB, sine at 4 kHz (2 octaves above the cutoff) | each step attenuates more than the one before | each step more than 8 dB; measured −30.8, −41.8, −54.4, −66.8 dB: `./Builds/PluginTests "Slope steepens the Diode Ladder step by step" -s \| grep "Diode slope"` | sst-filters Diode Ladder slopes |
| Diode Ladder step choice | slope values 6, 8.9, 9.1, 15, 21, 24 give the 6, 6, 12, 18, 24, 24 dB steps (nearest step, ties to the steeper) | exact | ADR 0008 |
| OB-Xd 4-pole, slope 6, 12, 18, 24 dB, sine at 4 kHz | attenuation grows with the slope, and slope 15 sits between 12 and 18 | each gap more than 3 dB; measured −16.9, −29.4, −34.7, −41.9, −54.4 dB for 6, 12, 15, 18, 24: `./Builds/PluginTests "Slope morphs the OB-Xd 4-pole continuously" -s \| grep "OB-Xd slope"` | sst-filters OB-Xd Morph |
| K35 Low-pass, saturation 0 % then 100 %, sine at 200 Hz, 0 dBFS, resonance 50 % | 100 % adds harmonics: third-harmonic level higher than at 0 % | more than 30 dB higher; measured −55.9 then −9.3 dB: `./Builds/PluginTests "K35 saturation adds harmonics" -s \| grep "third harmonic"` | sst-filters K35 tanh stage |
| Vintage variant, each of the 6, noise | finite output, peak under the task 0007 bound of 4 | as stated | task 0007 bound |
| Settings changed on every block (slope, saturation, variant, Model) through all values, 2 s of noise, resonance 100 % | every sample finite; RTSan build: no report | exact | `.claude/rules/realtime-audio.md` |
| State saved by task 0009 (no new parameter) | loads; new parameters take their defaults, other values kept | exact | `.claude/rules/juce.md` (fallback) |
| Every filter variant (Diode 4 slopes, Vintage 6 sub-models, the other entries), noise, at 44.1, 48, 96, 192 kHz, cutoff 20 Hz, 1 kHz, 20 kHz, resonance 0, 0.5, 1, extra value 0 and 1 | finite output, peak under 4 | measured largest peak 2.94: `./Builds/DspTests "Every filter variant stays finite at the bounds" -s \| grep "largest peak"` | task 0009 test, extended |
| `check`, `render`, `compare` | render cases unchanged (default entry Cytomic SVF Low-pass ignores the new parameters): `compare` green, no `golden:` commit | manifest tolerances | ADR 0003 |

## Out of scope

* Crossfade on a Model change, a Diode slope step or a Vintage variant change: a click is possible (crossfade task, next on the ROADMAP).
* OB-Xd "24 dB legacy" sub-model, Cytomic SVF passbands beyond the four, other sst-filters settings.
* A filter curve display: custom-drawn UI, SPEC Out of v1 item 5 (`FUTURE.md`).
* Hiding parameters a model does not use: the generic editor shows them all (ADR 0008).
* Drive, oversampling, LFO, mix, gain in.

## Decision notes

* One `MultimodeFilter` per **filter variant**: each Model entry, plus one per Diode Ladder slope and per Vintage variant (17 instances), configured in the constructor and prepared in `prepareToPlay`. `processBlock` selects one and resets it when the selection changes. No `configure` or `prepareInstance` on the audio thread.
* The variant choice (Model index, slope, Vintage variant to an instance) is a pure function in `dsp/`, tested without JUCE.
* The OB-Xd Morph and the K35 continuous saturation reach sst-filters as the `extra` argument of `makeCoefficients`, called on every packet, so they are smoothed with the cutoff and resonance.
* Search done (`.claude/rules/scope.md` rule 8): sst-filters already provides every setting; no new dependency.

## Rules to load

* `.claude/rules/juce.md`, `realtime-audio.md`, `dsp.md`, `testing.md`, `cpp.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [x] Tests written before the code, red then green (compile red on 9867411's tests; behaviour red with a stub `chooseFilter`, 5 of 18 dsp filter cases failing; plugin wiring red with the settings ignored on purpose in `processBlock`, 3 of 15 filter cases failing).
* [x] Debug, Release, RTSan: `ctest` green. Locally on 9867411 (Debug, GCC, WSL2): 63/63. CI run 41663616715 on 9867411: Linux and Windows (Release) and Linux RealtimeSanitizer (Debug, clang 20) green.
* [x] `check` green (`check: all passed`); `compare` green (`compare: all files within tolerance`), no `golden:` commit, `tests/golden/` untouched.
* [x] pluginval 10 green (CI run 41663616715, Windows and Linux).
* [x] `rt-safety-reviewer` passed (9867411): no finding.
* [x] `GLOSSARY.md` updated (Filter variant, Per-model setting, Slope, Morph, K35 saturation, Vintage variant, Extra value).
* [x] **Maxime: the CI installer loads in Live; Slope changes the OB-Xd smoothly and the Diode in steps, K35 saturation and Vintage variant change the sound, and the defaults sound like task 0009.** 2026-11-03 16:32 CET, installer `MoodzFilter-0.0.1-Windows.exe` from CI run 41663616715 (9867411): approved ("c'est bon").
* [ ] `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
