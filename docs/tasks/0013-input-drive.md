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
| Asymmetric, 12 dB, first sample 0.5, and first sample -0.5 after a reset | 0.7523, and -1.3392 | 1e-3 | Same |
| Both curves, 12 dB, sample 0.001 | 0.00398 (the drive gain: same small signal level) | 1e-5 | Same |
| Symmetric 3 dB, sample 0.5 | 0.5541 (half dry, half saturated) | 1e-3 | Decision notes |
| 1 kHz sine, −12 dBFS, Drive 0 to 24 dB in steps of 0.5 dB | RMS never falls as Drive rises; no step above 1.5 dB | to measure | No hole in the Drive travel |
| 1 kHz sine, 0 dBFS, same steps | the level dips under the dry level by less than 0.5 dB: saturation limits the peaks before the gain makes up for it | to measure | Code review; above full scale the dip is larger, that is the saturation working |
| Asymmetric, 24 dB, 0 dBFS, sines at 20 Hz, 40 Hz, 1 kHz | peak to peak under 2.3, the bass limited like the rest | to measure | Code review |
| Curve change and Oversampling change at 24 dB, 200 Hz sine, −12 and 0 dBFS | largest 5 ms mean under 0.01 for the curve, 0.005 for Oversampling, the sine averaged out | to measure | Code review; red before the fix: 0.07 to 0.09 |
| Drive 0 to 24 dB in one block, same signal | largest 5 ms mean under 0.2 Asymmetric, under 0.06 Symmetric (the amplitude rising, no offset) | to measure | The worst case, for the listening |
| 1 kHz sine, −12 dBFS, 12 dB | second harmonic: Symmetric under −100 dB, Asymmetric above −50 dB, against the fundamental | to measure | ADR 0009 (even harmonics) |
| Asymmetric, 24 dB, 1 kHz sine at −6 dBFS, last second of 2 s | mean of the output near 0 | to measure, against the run without the DC blocker | ADR 0009 (DC offset to remove) |
| Silence, 24 dB, both curves | silence | 1e-9 | A saturator adds nothing to nothing |
| Drive change 0 to 24 dB (sine), 6 to 24 dB (constant 0.01), curve change at 24 dB (sine) | equal to the target's output from 20 ms after the change, not before | ± 1 sample, 1e-6 | SPEC (Drive smoothed), same ramp time as Gain out |
| Drive 0 to 24 dB, constant 0.01, 480 samples into the ramp | 0.04703: gain and blend both half way | 1e-4 | Decision notes; the value comes from the Python command in the test |
| Drive and curve moved during playback in the plugin, 1 kHz sine | the output changes, and ends equal to a render set from the start | 1e-3 | Task 0013 verifier: the click bounds alone pass if a change never arrives |
| Chain, 100 Hz sine at −6 dBFS, 24 dB, Cytomic SVF Low-pass at 300 Hz | 9th harmonic under −40 dB against the fundamental: the filter comes after the drive | to measure | SPEC signal chain |
| `InputDrive` alone, a non-finite input sample or ±3e38, Asymmetric | later samples finite: the DC blocker keeps nothing | exact | `.claude/rules/dsp.md`, rt-safety review. The filter downstream is not covered: `FUTURE.md` |
| Stereo, different signals | each channel equals the same signal driven alone; silence next to a signal stays silence | 1e-7 | SPEC (linked stereo) |
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
* **DC removal.** On the even part of the Asymmetric curve alone, `shape = odd + even`, `odd = t / D`, `even = −tanh(b) t² / D`, `D = 1 − tanh²(b) t²`: the dry signal and the limiting odd part are never filtered, so `y = x + w (odd + HP(even) − x)` (a blocker on the whole curve overshoots in the bass). The DC blocker is `y[n] = x[n] − x[n−1] + R y[n−1]` (J. O. Smith, *Introduction to Digital Filters*, "DC Blocker"), `R = exp(−2π fc / fs)`, `fc = 10 Hz`. One per channel. It keeps running on Symmetric, so a curve change finds it settled, and an incoming oversampling path takes the outgoing path's DC estimate (code review: a cold blocker let the whole offset through, a low-frequency bump the click measure could not see). A Drive jump in one block still leaves a bump, since the offset appears faster than 10 Hz follows; bound in the tests, for Maxime to judge.
* **Smoothing.** Gain, `w` and the blend between the two curves ramp linearly over 20 ms, per sample. A curve change is therefore a 20 ms blend, with no reset and no second filter chain.
* **At rest.** At 0 dB nothing is computed and the DC blocker is cleared.
* **Place in the chain.** One `InputDrive` per oversampling path, between `up` and the filter bank, prepared at the path's rate. Each path ramps on its own; an incoming path starts at the target.
* **Search done** (`.claude/rules/scope.md` rule 8): `juce::dsp::WaveShaper` is not usable in `dsp/` (no JUCE). sst-waveshapers (GPL, allowed) and math_approx (BSD) are not added: two one-line curves do not justify a new dependency. `std::tanh` is kept until gate 2 measures its cost.

## Rules to load

* `.claude/rules/realtime-audio.md`, `dsp.md`, `juce.md`, `testing.md`, `cpp.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [x] Tests written before the code, red then green.
* [x] Debug, Release, RTSan: `ctest` green.
* [x] `check` and `compare` green, no golden change.
* [x] pluginval 10 green.
* [x] `rt-safety-reviewer` passed.
* [x] `GLOSSARY.md` updated (Input drive, Drive curve, DC blocker).
* [x] **Maxime: the CI installer loads in Live; at 0 dB nothing changes; Drive saturates progressively with no hole in its travel; the two curves sound different and both are usable; automating Drive and Drive curve during playback does not click.**
* [x] `code-review`, `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)

2026-11-05, task-verifier: pass on ccfb835, after 3 rounds that failed on test coverage and evidence, none on behaviour; rounds 2 and 3 only changed tests and docs, `git diff --stat b2901d9 ccfb835 -- source` is empty. Local Debug: `ctest` 127/127, `MoodzFilterTools check` all passed, `render` + `compare` against `tests/golden` within tolerance, no golden change on the branch. CI run 42010306741 on b21a164, same source: Linux and Windows with pluginval 10, RealtimeSanitizer, all green; the closing commit's run is linked in PR #16. Figures: second harmonic -238 dB Symmetric, -19.7 dB Asymmetric (`./Builds/DspTests "The asymmetric curve adds even harmonics\, the symmetric one does not" -s | grep second`); mean -6.9e-7 at 24 dB, -0.29 with `blocked = even` (`./Builds/DspTests "The asymmetric curve leaves no DC offset" -s | grep mean`); largest alias up to 20 kHz: Off -20.2, 2x -38.7, 4x -71.8 dB Symmetric (`./Builds/DspTests "Oversampling lowers the aliases of the drive" -s | grep alias`); Asymmetric span 2.20 at 20 Hz, 2.19 at 1 kHz (`./Builds/DspTests "The asymmetric curve limits as much in the bass" -s | grep span`); bump at most 0.0065 on a curve change, 0.0024 on an Oversampling change, 0.16 on a Drive jump in one block (`./Builds/PluginTests "[drive]" -s | grep bump`). The verifier's mutations all go red, except two accepted because no row asks for them: `fc` = 100 Hz and no clear at rest. rt-safety review of 5bc766b and b2901d9: no real-time problem. Code review on 5bc766b; b2901d9, which came out of it, had the rt-safety review and the verifier, not a second code review. Maxime's Live test on the installer of b2901d9: all fine on the five points, sudden Drive jump included, 2026-11-05 about 10:25 CET. Not covered by that listening: the Asymmetric tail when a signal stops abruptly, raised afterwards. Could not be proved from git: red before green for 7b173fb and 5bc766b, where tests and code share a commit; for the later tests, added after the code, the mutations are the evidence. Left in `FUTURE.md`: the non-finite filter state, one ramp type, benchmarking the chain, the offset tail, the combined Drive jump and factor change. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0013-input-drive.md`.
