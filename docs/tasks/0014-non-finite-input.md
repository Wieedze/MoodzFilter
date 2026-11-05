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
| Chain, every filter variant, every factor, resonance 100 %, Drive 0 and 24 dB, noise with one NaN, +inf, -inf, ±3e38 or ±FLT_MAX at sample 1000 | every output sample finite, including the bad one's | exact | `.claude/rules/dsp.md`; red before: non-finite to the end of the render |
| Same, Cytomic SVF Low-pass, 1 kHz, resonance 20 %, sine with one NaN | back to the output without the bad sample 100 ms later | to measure | The plugin plays on |
| NaN, ±inf, ±3e38, ±FLT_MAX, 256.5, noise, 2x, Cytomic SVF High-pass | same output as a 0, finite | 1e-6 | Decision notes; code review: clamped to the bound, a huge sample came out at about +48 dBFS |
| Every variant at 4x, a 7 kHz square at ±256, resonance 100 %, cutoff 20 Hz and 20 kHz, Drive 0 dB and 24 dB Asymmetric, 44.1 and 192 kHz | every sample finite | exact | The bound has to hold beyond one sample, code review |
| Stereo, bad sample on the left | right channel equal to its output without the bad sample | 1e-6 | SPEC (linked settings, separate signals) |
| Every input sample within ±256 (+48 dBFS) | output identical with and without the guard: every golden reference unchanged | manifest tolerances | Legitimate hot signals are not touched |
| Plugin, one NaN or +inf in `processBlock`, Gain out at silence and at 0 dB | every output sample finite after the latency | exact | FUTURE line from task 0006 |
| 2 s of noise with a bad sample on every block, all settings cycling, RTSan build | every sample finite; no report | exact | `.claude/rules/realtime-audio.md` |

## Out of scope

* Host bypass: it only delays the input, so a bad sample comes out 40 samples later and leaves no state behind.
* A filter that becomes unstable on its own from finite input: nothing shows it today.
* Any change of sound on valid signals.

## Decision notes

* **Where.** Once, at the top of `OversampledFilterChain::process`: everything downstream (oversampler, drive, filter) only ever sees bounded input. Gain out comes after the chain, so it is covered too.
* **What.** Anything that is not within ±256 (+48 dBFS) becomes 0, NaN and infinities included: far above any real signal, so it is not a signal. Not clamped: at the bound a bad sample would come out as a click of about +48 dBFS, which is what the first version did (code review). The filters keep a finite state up to the bound, which the tests check.
* **Search done** (`.claude/rules/scope.md` rule 8): JUCE's `FloatVectorOperations` have no NaN handling and are not usable in `dsp/`; a loop of a few lines needs no library.
* **Cost.** One comparison per sample per channel, at the base rate.

## Rules to load

* `.claude/rules/realtime-audio.md`, `dsp.md`, `testing.md`, `cpp.md`, `workflow.md`

## Skills

* `tdd`

## Definition of done

* [ ] Tests written before the code, seen red.
* [ ] Debug, Release, RTSan: `ctest` green.
* [ ] `check` and `compare` green, no golden change.
* [ ] pluginval 10 green.
* [ ] `rt-safety-reviewer` passed.
* [ ] Mutations on the new tests: removing the guard, or each of its two branches, goes red.
* [ ] `FUTURE.md` line on non-finite input removed.
* [ ] `code-review`, `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
