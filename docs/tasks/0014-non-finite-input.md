# 0014 — Keep one bad input sample from silencing a channel

**Phase**: 2
**Layer**: dsp (oversampled filter chain), tests
**Depends on**: 0013

## Goal

One NaN, infinite or absurdly large input sample no longer leaves the filter state non-finite and no longer comes out as a click: the plugin plays on, without a reset.

## Expected behaviour (measurable)

Unless stated: 48 kHz, blocks of 512. Every bound marked "to measure" comes from a run on the test's own signal, with its command in the test; the red run gives the before.

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Chain, every filter variant, every factor, resonance 100 %, Drive 0 and 24 dB, noise with one NaN, +inf, -inf, ±3e38 or ±FLT_MAX at sample 1000 | every output sample finite, including the bad one's | exact | `.claude/rules/dsp.md`; red before: NaN non-finite to the end on every run, the others on some variants |
| Same, Cytomic SVF Low-pass, 1 kHz, resonance 20 %, sine with one NaN | back to the output without the bad sample 100 ms later | 1e-4 (run: 0) | The plugin plays on |
| NaN, ±inf, ±3e38, ±FLT_MAX, ±256.5, noise, 2x, Cytomic SVF High-pass | same output as a 0, finite | 1e-6 | Decision notes; code review: clamped to the bound, a huge sample came out as a loud click, as the peak printed by the test shows |
| Every variant at 4x, a 7 kHz square at ±256, resonance 100 %, cutoff 20 Hz and 20 kHz, Drive 0 dB and 24 dB Asymmetric, 44.1 and 192 kHz | every sample finite | exact | The bound has to hold beyond one sample, code review |
| Stereo, bad sample on the left | right channel equal to its output without the bad sample | 1e-6 | SPEC (linked settings, separate signals) |
| Every input sample within ±256 (+48 dBFS) | output identical with and without the guard: every golden reference unchanged | manifest tolerances | Legitimate hot signals are not touched |
| Plugin, one NaN or +inf in `processBlock`, Gain out at silence and at 0 dB | every output sample finite after the latency | exact | FUTURE line from task 0006 |
| 2 s of noise with a bad sample on every block, on its first, last or a middle sample, either channel, all settings cycling, RTSan build | every sample finite; no report | exact | `.claude/rules/realtime-audio.md` |

## Out of scope

* Host bypass: it only delays the input, so a bad sample comes out of the delay, once, and reaches neither the filter nor Gain out. The output can be non-finite while bypassed (`FUTURE.md`).
* A filter that becomes unstable on its own from finite input: nothing shows it today.
* Any change of sound on valid signals.

## Decision notes

* **Where.** Once, at the top of `OversampledFilterChain::process`: everything downstream (oversampler, drive, filter) only ever sees bounded input. Gain out comes after the chain, so it is covered too.
* **What.** Anything that is not within ±256 (+48 dBFS, the command is at `maxInputSample`) becomes 0, NaN and infinities included: far above any real signal, so it is not a signal. Not clamped: at the bound a bad sample comes out as a loud click, which is what the first version did (code review; the test prints the peak). The filters keep a finite state up to the bound, which the tests check. A resonant instance fed a very hot signal can peak above 256, and a second instance in series would then silence those samples (`FUTURE.md`).
* **Search done** (`.claude/rules/scope.md` rule 8): JUCE's `FloatVectorOperations` have no NaN handling and are not usable in `dsp/`; a loop of a few lines needs no library.
* **Cost.** One comparison per sample per channel, at the base rate.

## Rules to load

* `.claude/rules/realtime-audio.md`, `dsp.md`, `testing.md`, `cpp.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [x] Tests seen red; see the End note for what git can and cannot show.
* [x] Debug, Release, RTSan: `ctest` green.
* [x] `check` and `compare` green, no golden change.
* [x] pluginval 10 green.
* [x] `rt-safety-reviewer` passed.
* [x] Mutations on the new tests: removing the guard, or each of its two branches, goes red.
* [x] `FUTURE.md` line on non-finite input removed.
* [x] `code-review`, `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)

2026-11-05, task-verifier: round 2 on 7835353 closed all 8 items of round 1 and all its mutants go red; its one remaining item, a figure without a command in `FUTURE.md`, is removed in the closing commit as it proposed, with no third round. Local Debug: `ctest` 135/135, `MoodzFilterTools check` all passed, `render` + `compare` against `tests/golden` within tolerance, no golden change. CI run 42024729546 on 7835353: Linux and Windows with pluginval 10, RealtimeSanitizer 136/136, all green. Figures: a bad sample gives back the clean output 100 ms later, difference 0 (`./Builds/DspTests "The chain plays on after a bad input sample" -s | grep "difference"`); a sample at the bound peaks at +47.31 dBFS on Cytomic SVF High-pass (`./Builds/DspTests "A bad sample counts as 0" -s | grep "peak"`); the body of 17616d9 says "about +48 dBFS" without that limit, corrected by the message of 7835353 since force pushes are forbidden. rt-safety review of b26a27f and 17616d9: no real-time problem. Code review on b26a27f only; 17616d9 and 7835353, which came out of it, had the rt-safety review and the verifier, not a second code review. No listening: a sample within the bound is never written, and every golden reference is unchanged. The references cover the gain cases with the filter wide open; other settings rest on the guard's logic and the hot signal test. Tests and guard landed together in b26a27f, so git cannot show red before green; the red run was seen during the work, and the mutations are the evidence. Left in `FUTURE.md`: non-finite output while bypassed (reaches no filter state), a second instance after a resonant one, the DC blocker's own finite checks. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0014-non-finite-input.md`.
