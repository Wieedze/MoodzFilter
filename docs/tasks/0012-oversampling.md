# 0012 — Oversample the filter, with one constant latency

**Phase**: 2
**Layer**: dsp (oversampler, oversampled filter chain), plugin (parameter, processor, latency, bypass), tests, build (Signalsmith DSP), golden
**Depends on**: 0011, playbook ADR 0009

## Goal

The filter runs at 1x, 2x or 4x according to a new Oversampling parameter (default 2x). The plugin reports one latency at every setting, and changing the setting during playback crossfades without a click and without the host realigning.

## Expected behaviour (measurable)

Unless stated: 48 kHz, stereo, blocks of 512. The **click measure** is the one of task 0011. Every bound marked "to measure" comes from a run on the test's own signal, with its command in the test; the red run gives the before.

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Oversampler round trip 2x and 4x, sines at 1, 10 and 18 kHz, −12 dBFS | gain 0 dB | ± 0.1 dB | ADR 0009; below 0.43 × the rate (20.6 kHz here), the pass band of `Oversampler2xFIR` |
| Oversampler round trip, impulse | peak exactly at the stage's reported latency, a whole number of samples | exact | ADR 0009 |
| Oversampler up then down, 2x, tone above the pass band at the high rate (36 kHz at 96 kHz) | attenuated by the down stage | at most −80 dB (run: −84.3 dB) | `.claude/rules/dsp.md`, anti-aliasing |
| `getLatencySamples()` at Off, 2x, 4x and at 44.1, 48, 96 kHz | the same value, equal to the 4x chain latency | exact | ADR 0009 |
| Off, 2x, 4x, Cytomic SVF Low-pass, cutoff 20 kHz, resonance 0, impulse | peak of the output at the reported latency in the 3 settings | ± 1 sample | ADR 0009 |
| Off, 2x, 4x, Cytomic SVF Low-pass, cutoff 1 kHz, resonance 20 %, sines at 500 Hz, 1 kHz and 4 kHz | same response between settings | ± 0.5 dB (run: largest 0.35 dB, 4 kHz at 4x) | SPEC signal chain (the cutoff does not move with the factor) |
| Every filter variant at 4x, extra 0 and 1, resonance 100 %, noise | every sample finite | exact | `.claude/rules/dsp.md` |
| Oversampling change Off to 2x, 2x to 4x, 4x to Off at a block boundary, 200 Hz sine, cutoff 5 kHz | click measure under 0.00025 (run: 0.00017, the sine alone), and at cutoff 20 kHz at −12 and 0 dBFS at most 20 % above the sine alone | the tests fail with `preRoll` forced to 0 in `OversampledFilterChain.cpp` (0.0012 to 0.0017 at cutoff 5 kHz) | SPEC success criterion 3, task 0012 code review |
| Same change, outgoing chain's contribution | 0 from 20 ms after the change | ± 1 sample | same ramp time as task 0011 |
| Oversampling and Model changed on every block through all values, 2 s of noise, resonance 100 % | every sample finite; RTSan build: no report | exact | `.claude/rules/realtime-audio.md` |
| Chain, block sizes 1, 7, 64, 480, 512, 2048, a factor change and a Model change | identical output whatever the block size | 1e-6 | `.claude/rules/dsp.md` |
| Bypass in the host | input delayed by the reported latency, per channel | exact | playbook `docs/07_REUSE.md` (uncompensated bypass, to avoid) |
| A state saved before this task | loads with Oversampling at its default 2x, other values kept | exact | `.claude/rules/juce.md` |
| `check`, `render`, `compare` | every render changes (default 2x, delay): `compare` red until a separate `golden:` commit, after Maxime's listening | manifest tolerances | ADR 0003 |

## Out of scope

* Drive and Drive curve: next task. Nothing non-linear is added here.
* Dry/wet mix and the dry path: Mix task.
* A smooth bypass toggle: a click when toggling the host's bypass goes to `FUTURE.md`.
* A second oversampling filter quality or a user-chosen filter length.
* Reversing a crossfade (`FUTURE.md`, task 0011).

## Decision notes

* Search done (`.claude/rules/scope.md` rule 8): `juce::dsp::Oversampling` is not usable in `dsp/` (no JUCE); Signalsmith DSP `rates.h` (MIT, v1.7.3) is the playbook's default for `dsp/`; `up` and `down` do not allocate, `resize` does (`prepare` only). HIIR is IIR, so not linear phase, a problem for the future Mix.
* Each factor owns one crossfading filter bank prepared at its rate, its stages and its delay to the common latency. A factor change resets the incoming chain, lets it settle unheard for 80 samples (the pre-roll, added after the code review: the empty start clicked with the filter open), then crossfades the two chains' aligned outputs linearly, all within 20 ms; a change during a crossfade waits for it to end, as in task 0011.
* Blocks are split in fixed internal chunks, so no allocation depends on the host's block size.
* Coefficient smoothing in sst-filters runs per packet, so it runs faster at a higher factor. To check at the listening; a bound is measured on the response only.
* `WholeSampleDelay` is own code: Signalsmith's delay classes bring interpolation and resizing that a fixed whole-sample delay does not need.
* The cost of 4x, and of two chains during a crossfade, is measured at gate 2.

## Rules to load

* `.claude/rules/realtime-audio.md`, `dsp.md`, `juce.md`, `testing.md`, `cpp.md`, `build.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [ ] Tests written before the code, red then green.
* [ ] Debug, Release, RTSan: `ctest` green.
* [ ] `check` green; `compare` green after a `golden: ` commit approved by Maxime.
* [ ] pluginval 10 green.
* [ ] `rt-safety-reviewer` passed.
* [ ] `GLOSSARY.md` updated (Oversampling factor, Oversampled filter chain, Constant latency, Pre-roll, Whole sample delay).
* [ ] `THIRD_PARTY_NOTICES.txt` updated (Signalsmith DSP).
* [ ] **Maxime: the CI installer loads in Live; the three settings sound right, at the same level; automating Oversampling during playback does not click; the track stays aligned (no shift against another track when changing the setting); bypass does not shift the track.**
* [ ] `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
