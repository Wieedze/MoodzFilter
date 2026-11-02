# 0003 — Smoothed output gain, with TDD

**Phase**: 1
**Layer**: dsp, plugin
**Depends on**: 0002

## Goal

The first vertical slice through every layer: the SPEC's `Gain out` parameter (−24 to +24 dB, default 0 dB, smoothed) declared in `plugin/Parameters.*`, copied into a `Params` snapshot per block, applied by a `dsp` processor `SmoothedGain`, with a versioned saved state.

## Expected behaviour (measurable)

| Test signal | Parameters | Expected result | Tolerance | Source of the expected value |
| --- | --- | --- | --- | --- |
| 1 kHz sine, peak 0.25 (−12 dBFS), 48 kHz, after `reset()` | gain +6 dB | peak 0.25 × 10^(6/20) = 0.498816 (computed with Python: `0.25*10**(6/20)`) | ± 1e-5 | definition of the dB, 20·log10 |
| same | gain −24 dB | peak 0.25 × 10^(−24/20) = 0.0157739 | ± 1e-6 | same |
| DC at 1.0, 48 kHz | 0 dB, then +6 dB | output never jumps by more than (1.995262 − 1) / 960 = 0.0010367 per sample, reaches 1.995262 after 960 samples (20 ms) and stays there | ± 1e-5 | linear ramp of 20 ms, 48 000 × 0.02 = 960 samples |
| DC at 1.0 | target set to +60 dB / −60 dB | clamped to 10^(24/20) = 15.848932 / 10^(−24/20) = 0.0630957 | ± 1e-4 | SPEC range −24 to +24 dB |
| DC at 1.0 | target set to NaN, then +∞ | ignored: output stays at the previous gain, all samples finite | exact finiteness, gain ± 1e-6 | `.claude/rules/dsp.md`, finite output |
| Same 4 096 samples of noise with a gain change in the middle | blocks of 1, 7, 64, 480, 512 samples | identical outputs | ± 1e-7 | block size independence rule |
| block of 0 samples | any | no crash, state unchanged | — | `.claude/rules/realtime-audio.md` |
| `PluginProcessor`, stereo, `Gain out` = +6 dB, after `prepareToPlay` | 0.25 constant input | output 0.498816 on both channels | ± 1e-5 | as above |
| Parameter ID list | — | exactly `{ "gainOut" }` | exact | `.claude/rules/juce.md`, frozen IDs |
| State round trip | `Gain out` = −7.5 dB, save, load into a new instance | −7.5 dB, state carries `stateVersion="1"` | ± 0.01 dB | `.claude/rules/juce.md` |
| State without `stateVersion` attribute | `Gain out` = 3 dB | loads, value 3 dB (missing version treated as 1) | ± 0.01 dB | documented fallback |

## Out of scope

* Gain in, mix, drive, filter, LFO, oversampling: later tasks of phases 2 and 3.
* `tools render`, `compare`, golden references (task 0004).
* `[[clang::nonblocking]]` and the RealtimeSanitizer job: they need clang ≥ 20, which this machine does not have (clang 18), so they go together into the "Green CI" task where CI can prove them.
* Any editor change: the generic editor arrives with the parameters it shows (the template editor stays for now).

## Reuse search (`.claude/rules/scope.md` rule 8)

`juce::SmoothedValue` does the job but `dsp/` cannot see JUCE. sst-basic-blocks has lag and smoothing classes (GPL v3, an ADR would be needed for a 20-line linear ramp). Decision: an in-house linear ramp of a few lines in `dsp/`, no ADR (obvious choice).

## Rules to load

* `.claude/rules/realtime-audio.md`, `cpp.md`, `dsp.md`, `juce.md`, `testing.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [x] Each row of the table above is a test, written and seen red before the code.
* [x] `cmake --build` with no new warning (Debug and Release).
* [x] `ctest` green.
* [x] pluginval 10 green.
* [x] `rt-safety-reviewer` passed (no finding on the audio path; its 3 defensive suggestions applied in bf04265).
* [x] `GLOSSARY.md` up to date (no new term: Params, Smoothing, Parameter ID, State already defined).
* [x] CI green (run 41488213810 on bf04265, Linux and Windows).
* [ ] `task-verifier`: pass.

## End note (filled by the task-verifier)
