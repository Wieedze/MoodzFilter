# 0015 — Move the cutoff with an LFO locked to the host tempo

**Phase**: 2
**Layer**: dsp (synced LFO, oversampled filter chain), plugin (parameters, processor), tests
**Depends on**: 0014

## Goal

LFO shape, LFO rate and LFO depth parameters move the cutoff in time with the host: when Live plays, the LFO follows the song position, so it stays in place across stops, loops and block sizes. At depth 0 the plugin sounds exactly as before.

## Expected behaviour (measurable)

Unless stated: 48 kHz, blocks of 512. Every bound marked "to measure" comes from a run on the test's own signal, with its command in the test; the red run gives the before.

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Shapes at phases 0, 1/8, 1/4, 1/2, 3/4 | sine, triangle, sawtooth, square at their definitions below | 1e-6 | SPEC |
| Every rate at 120 BPM | one cycle lasts its length in quarter notes × 0.5 s | 1 sample | SPEC |
| Beat 0 and every multiple of the cycle length, positive or negative position | phase 0 | 1e-9 | Decision notes |
| Tempo 0, negative, NaN, infinite; position NaN, infinite | ignored, the LFO keeps running at the last valid tempo, 120 BPM at first | exact | `.claude/rules/dsp.md` |
| `check`, `render`, `compare` at depth 0 | every reference unchanged, no `golden:` commit | manifest tolerances | ADR 0003 |
| Chain, depth 0 against the chain before the task, any shape and rate | same output | 1e-7 | Goal |
| Chain, 2 kHz sine, Cytomic SVF Low-pass at 500 Hz, depth +2 octaves, 1/4 at 120 BPM | level highest around the positive peak, a period of 0.5 s; negated at depth −2 octaves | to measure | SPEC (depth 0 to ±4 octaves on the cutoff) |
| Same, cutoff near a bound, depth ±4 octaves | every sample finite, the cutoff held within 20 Hz to 20 kHz | exact | `.claude/rules/dsp.md` |
| Chain, LFO active, block sizes 1, 7, 64, 480, 512, 2048, every shape | identical output | 1e-6 | `.claude/rules/dsp.md`, ROADMAP |
| Plugin, a host position moving regularly, block sizes 64, 512, 2048 | identical output | 1e-5 | ROADMAP |
| Plugin, playback started at beat 8 against at beat 0 | same output once the filter has settled | to measure | Goal: locked to the song position |
| Plugin, host stopped or with no play head | the LFO runs on at the last tempo, output changes | exact | Decision notes |
| Plugin, tempo moved during playback | the LFO's period follows | to measure | SPEC |
| Depth change, 200 Hz sine | reaches its target 20 ms after the change, not before | ± 32 samples | SPEC (depth smoothed) |
| Square and sawtooth at ±4 octaves, Rate, Shape and position jump (loop), 200 Hz sine, cutoff 1 kHz, −12 and 0 dBFS | click measure against the steady filter, bound to measure, and the same test without the slew goes red | to measure | SPEC success criterion 3 |
| LFO moved during playback in the plugin | the output changes: a bound alone does not prove it arrives | exact | Post-mortem 0013 |
| All LFO parameters, Model, Oversampling, Drive moving on every block, 2 s of noise, RTSan | every sample finite; no report | exact | `.claude/rules/realtime-audio.md` |
| Parameters | LFO shape, LFO rate (22 values, frozen order, default 1/4), LFO depth −4 to +4 octaves, default 0; version 6; frozen list | exact | SPEC, `.claude/rules/juce.md` |
| A state saved before this task | loads with the defaults, other values kept | exact | `.claude/rules/juce.md` |

## Out of scope

* A random shape, other destinations, a stereo phase offset: `FUTURE.md` or SPEC Out of v1.
* A phase parameter, or a restart on a MIDI note.
* Bars following the host's time signature: lengths are counted in quarter notes (Decision notes).
* Sample-accurate host automation.
* The CPU budget: gate 2.

## Decision notes

* **Definitions**, phase `p` in [0, 1): sine `sin(2π p)`; triangle `4p` up to 1/4, `2 − 4p` up to 3/4, `4p − 4` after, so it starts at 0 rising like the sine; sawtooth `2p − 1` rising; square +1 for the first half, −1 for the second.
* **Rates**, longest to shortest, in quarter notes: 4 bars 16, 2 bars D 12, 4 bars T 10.67, 2 bars 8, 1 bar D 6, 2 bars T 5.33, 1 bar 4, 1/2 D 3, 1 bar T 2.67, 1/2 2, 1/4 D 1.5, 1/2 T 1.33, 1/4 1, 1/8 D 0.75, 1/4 T 0.67, 1/8 0.5, 1/16 D 0.375, 1/8 T 0.33, 1/16 0.25, 1/32 D 0.1875, 1/16 T 0.167, 1/32 0.125. A bar is 4 quarter notes in every time signature, so a rate means the same length in any song; the musician picks a rate that fits a 3/4 song.
* **Sync.** While the host plays, each block sets the phase from its position: `p = frac(position / length)`, so the LFO is where the song says after a stop or a loop. While stopped, or without a play head, it runs freely from where it is, at the last known tempo, 120 BPM at first.
* **Grid.** The cutoff updates every 32 base samples, counted across blocks, so the output does not depend on the block size. The chain splits its chunks on that grid. Below 1 ms at every supported rate up to 48 kHz; sst-filters then smooths the coefficients per packet.
* **Slew.** A 2 ms one-pole on the LFO value before the depth: a square edge, a sawtooth reset, a Rate or Shape change and a loop all become a short curve, instead of a jump of up to 8 octaves. Time constant to confirm by the click test and the listening.
* **Cutoff.** `cutoff × 2^(depth × value)`, clamped by the filter to 20 Hz to 20 kHz. At depth 0 and at rest, nothing is computed: the cutoff is the parameter itself, so the references do not move.
* **Depth smoothing.** A linear ramp over 20 ms, stepped on the grid.
* **Search done** (`.claude/rules/scope.md` rule 8): `juce::dsp::Oscillator` is not usable in `dsp/` (no JUCE) and knows nothing of a song position; a phase and four one-line shapes need no library. An LFO needs no anti-aliasing here, since it only moves a coefficient.

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
* [ ] Mutations run on the new tests before the verifier: each call, each ramp, each shape, the sync and the grid removed in turn go red.
* [ ] `GLOSSARY.md` updated (LFO, Cycle length, Modulation grid, Slew).
* [ ] **Maxime: the CI installer loads in Live; at depth 0 nothing changes; the four shapes are heard and sit on the beat at several rates and tempos; after a stop and a loop the LFO is back on the beat; positive and negative depth move in opposite directions; square at high depth does not click in a way that bothers.**
* [ ] `code-review`, `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
