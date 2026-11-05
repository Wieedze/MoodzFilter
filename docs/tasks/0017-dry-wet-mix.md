# 0017 — Mix the dry signal with the filtered one

**Phase**: 2
**Layer**: dsp (dry/wet mix, oversampled filter chain), plugin (parameters, processor), tests
**Depends on**: 0016, playbook ADR 0009

## Goal

A Mix parameter (0 to 100 %, default 100 %) blends the input with the driven and filtered signal, aligned so the blend creates no comb. At 100 % the plugin sounds exactly as before.

## Expected behaviour (measurable)

Unless stated: 48 kHz, blocks of 512. Every bound marked "to measure" comes from a run on the test's own signal, with its command in the test.

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Mix 100 %, against the chain before the task | same output | 1e-7 | Goal |
| `check`, `render`, `compare` at the default | every reference unchanged, no `golden:` commit | manifest tolerances | ADR 0003 |
| Mix 0 %, any Model, Drive, Oversampling, LFO | output equal to the input delayed by the constant latency, 40 samples, the same as bypass | 1e-6 | SPEC, ADR 0009 |
| Mix 50 %, an impulse, Oversampling Off, 2x, 4x, filter wide open | a single peak at the latency, ± 1 sample of the wet peak: nothing beyond 2 samples above 40 % of it (run: at most 3 %) | to measure | SPEC: no comb filtering |
| Mix 50 %, sines at 1, 5, 10 and 15 kHz, filter wide open, resonance 0, each factor | within 2 dB of the blend of a dry at 0 dB and that wet's level (run: at most 1.78 dB at 15 kHz at 4x, the filter's own phase near its cutoff) | to measure | SPEC |
| Mix at 25 % and 75 %, a 10 kHz sine, Cytomic SVF Low-pass at 20 Hz, so the wet part is gone | output level 75 % and 25 % of the input: the dry weight | 0.1 dB | Linear blend, decision notes |
| Mix change | reaches its target 20 ms after the change, not before | ± 1 sample | SPEC (Mix smoothed), same ramp as Gain out |
| Same at 44.1 kHz | the same time | ± 1 sample | Post-mortem 0015 |
| Chain, block sizes 1, 7, 64, 480, 512, 2048, a Mix change | identical output | 1e-6 | `.claude/rules/dsp.md` |
| Bad input samples at Mix 0 % and 50 % | every output finite: the dry path is guarded too | exact | Task 0014 |
| Stereo, different signals | each channel equals the same signal mixed alone | 1e-6 | SPEC |
| Mix 0 to 100 % and back at Drive 24 dB Asymmetric, filter wide open, −12 and 0 dBFS | click measure at most 20 % above the higher setting held steady, and red with a one-sample ramp; bump measure under 0.06 (run: 0.05, the amplitude changing) | to measure | SPEC success criterion 3 |
| Mix moved during playback in the plugin | the output changes and ends equal to a render set from the start | 1e-3 | Post-mortem 0013 |
| Every parameter moving on every block, 2 s of noise, a bad sample per block, RTSan | every sample finite; no report | exact | `.claude/rules/realtime-audio.md` |
| Parameters | Mix 0 to 100 %, default 100 %, a non-finite value ignored; version 7; frozen list | exact | SPEC, `.claude/rules/juce.md` |
| A state saved before this task | loads with Mix 100 %, other values kept | exact | `.claude/rules/juce.md` |
| `getLatencySamples()` | still 40 | exact | ADR 0009 |

## Out of scope

* Gain in and smoothing everywhere: next task.
* Equal-power blending or a choice of law.
* A phase-compensated dry path following the filter's own phase: only the latency is aligned.
* A click-free Bypass.

## Decision notes

* **Blend.** `y = m wet + (1 − m) dry`, linear in `m`: dry and wet are aligned and correlated, so an equal-power law would bump the level in the middle.
* **Where.** At the end of each segment of `OversampledFilterChain::process`, after the factor crossfade. The dry part is a copy of the input after the input guard, delayed by the constant latency (`WholeSampleDelay`, 40). Gain out stays after it, as in the SPEC.
* **At rest.** At 100 % the output is the wet part untouched, by a shortcut, and the dry delay keeps running so a Mix change finds it filled. At 0 % the filter keeps running too, so moving Mix up finds it settled: no saving there, and none is sought.
* **Smoothing.** A linear ramp over 20 ms, per sample. A fourth ramp, added to the `FUTURE.md` line on one ramp type: no refactor in this task.
* **Search done** (`.claude/rules/scope.md` rule 8): `juce::dsp::DryWetMixer` is not usable in `dsp/` (no JUCE). Its latency handling is what `WholeSampleDelay` already does here.
* **Comb.** The filter has its own phase, so at Mix 50 % a low-pass near its cutoff will thin some frequencies. That is the sound of a dry/wet filter, not a misalignment. The comb rows run wide open.

## Rules to load

* `.claude/rules/realtime-audio.md`, `dsp.md`, `juce.md`, `testing.md`, `cpp.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [ ] Tests written before the code, red then green.
* [ ] Debug, Release, RTSan: `ctest` green.
* [ ] `check` and `compare` green, no golden change.
* [ ] pluginval 10 green.
* [ ] `rt-safety-reviewer` passed.
* [ ] Mutations run before the verifier: each call, the ramp, the delay length, the guard on the dry path, at 44.1 kHz too.
* [ ] `GLOSSARY.md` updated (Mix, Dry path).
* [ ] **Maxime: the CI installer in Live; at Mix 100 % nothing changes; at Mix 0 % the sound is the dry track, aligned with another copy of it; between them no hollow, comb-like sound on a wide-open filter; automating Mix does not click.**
* [ ] `code-review`, `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
