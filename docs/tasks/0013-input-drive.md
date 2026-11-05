# 0013 — Add the input drive and its two curves

**Phase**: 2
**Layer**: dsp (input drive, oversampled filter chain), plugin (parameters, processor), tests
**Depends on**: 0012, playbook ADR 0009

## Goal

A Drive parameter (0 to 24 dB) saturates the signal before the filter, inside the oversampling, with a Drive curve parameter (Symmetric by default, Asymmetric). At 0 dB the plugin sounds exactly as before.

## Expected behaviour (measurable)

Unless stated: 48 kHz, blocks of 512. Every bound marked "to measure" comes from a run on the test's own signal, with its command in the test; the red run gives the before.

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Drive 0 dB, both curves, noise | output equal to the input | 1e-7 | ADR 0009, transparent at 0 dB |
| `check`, `render`, `compare` with Drive at its default | every reference unchanged, no `golden:` commit | manifest tolerances | ADR 0003 |
| Symmetric, 12 dB, single samples 0.5 and -0.5 | ±0.9633 = ±tanh(10^(12/20) × 0.5) | 1e-4 | Curve definition below; the value comes from the Python command in the test |
| Asymmetric, 12 dB, first sample 0.5, then -0.5 | 0.7523, then -1.3390 | 1e-3 | Same |
| Both curves, 12 dB, sample 0.001 | 0.00398 (the drive gain: same small signal level) | 1e-5 | Same |
| Symmetric 3 dB, sample 0.5 | 0.5541 (half dry, half saturated) | 1e-3 | Decision notes |
| 1 kHz sine, −12 dBFS, Drive 0 to 24 dB in steps of 0.5 dB | RMS never falls as Drive rises, no jump at 6 dB | to measure | No hole in the Drive travel |
| 1 kHz sine, −12 dBFS, 12 dB | second harmonic: Symmetric under −100 dB, Asymmetric above −50 dB, against the fundamental | to measure | ADR 0009 (even harmonics) |
| Asymmetric, 24 dB, 1 kHz sine at −6 dBFS, last second of 2 s | mean of the output near 0 | to measure, against the run without the DC blocker | ADR 0009 (DC offset to remove) |
| Silence, 24 dB, both curves | exactly silence | exact | A saturator adds nothing to nothing |
| Drive change, sine | equal to the target's output from 20 ms after the change, not before | ± 1 sample, 1e-6 | SPEC (Drive smoothed), same ramp time as Gain out |
| Non-finite input sample, Asymmetric | later samples finite: the DC blocker keeps no NaN | exact | `.claude/rules/dsp.md` |
| Stereo | one channel's signal does not reach the other; the same settings on both | 1e-7 | SPEC (linked stereo) |
| Chain at Off, 2x, 4x, 24 dB, 5 kHz sine, cutoff 20 kHz | largest alias up to 20 kHz: 2x under Off and 4x under 2x, by at least 6 dB each; 4x under −70 dB against the fundamental | to measure | `.claude/rules/dsp.md` (nonlinearities are oversampled) |
| Chain, block sizes 1, 7, 64, 480, 512, 2048, with a Drive change and a factor change | identical output | 1e-6 | `.claude/rules/dsp.md` |
| Drive 0 to 24 dB, curve change at 24 dB, Oversampling change at 24 dB, 200 Hz sine, cutoff 20 kHz | click measure at most 20 % above the same setting held steady | to measure | SPEC success criterion 3, post-mortem 0012 (filter wide open, full level) |
| Drive, Drive curve, Oversampling and Model changed on every block, 2 s of noise, resonance 100 % | every sample finite; RTSan build: no report | exact | `.claude/rules/realtime-audio.md` |
| Parameters | Drive 0 to 24 dB, default 0; Drive curve Symmetric, Asymmetric, default Symmetric; version 5; added to the frozen list | exact | SPEC, `.claude/rules/juce.md` |
| A state saved before this task | loads with Drive 0 dB and Symmetric, other values kept | exact | `.claude/rules/juce.md` |
| `getLatencySamples()` | still 40 samples | exact | ADR 0009 |

## Out of scope

* Automatic level compensation: Drive makes the signal louder until it saturates. Gain out and the coming Gain in handle the level.
* Gain in, Mix and the delayed dry path: next tasks.
* A third curve, or a user-chosen bias.
* New golden references for Drive: gate 2.
* The CPU budget: gate 2.

## Decision notes

ADR 0009 leaves the DC removal and the smoothing to this task.

* **Curves.** With `g` the drive gain and `u = g x`: Symmetric `tanh(u)`. Asymmetric `(tanh(u + b) − tanh(b)) / (1 − tanh²(b))`, `b = 0.3`: a biased tanh, shifted back to 0 at 0 and divided by its slope at 0, so both curves have the same small signal gain and silence stays silence. Written `t / (1 + t tanh(b))`, `t = tanh(u)` (the tanh addition formula): the same curve, exactly 0 at 0 in floats, one tanh per sample shared by both curves.
* **Transparent at 0 dB.** tanh alone is not transparent, so `y = x + w (shape(u) − x)`, where `w` rises linearly from 0 at 0 dB to 1 at 6 dB. Dry and saturated are in phase, so the blend has no comb.
* **DC removal.** Only on the Asymmetric part `shape(u) − x`, so the dry signal is never filtered: a DC blocker `y[n] = x[n] − x[n−1] + R y[n−1]` (J. O. Smith, *Introduction to Digital Filters*, "DC Blocker"), `R = exp(−2π fc / fs)`, `fc = 10 Hz`. One per channel.
* **Smoothing.** Gain, `w` and the blend between the two curves ramp linearly over 20 ms, per sample. A curve change is therefore a 20 ms blend, with no reset and no second filter chain.
* **At rest.** At 0 dB nothing is computed and the DC blocker is cleared; on Symmetric the Asymmetric curve is not computed.
* **Place in the chain.** One `InputDrive` per oversampling path, between `up` and the filter bank, prepared at the path's rate. Each path ramps on its own; an incoming path starts at the target.
* **Search done** (`.claude/rules/scope.md` rule 8): `juce::dsp::WaveShaper` is not usable in `dsp/` (no JUCE). sst-waveshapers (GPL, allowed) and math_approx (BSD) are not added: two one-line curves do not justify a new dependency. `std::tanh` is kept until gate 2 measures its cost.

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
* [ ] `GLOSSARY.md` updated (Input drive, Drive curve, DC blocker).
* [ ] **Maxime: the CI installer loads in Live; at 0 dB nothing changes; Drive saturates progressively with no hole in its travel; the two curves sound different and both are usable; automating Drive and Drive curve during playback does not click.**
* [ ] `code-review`, `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
