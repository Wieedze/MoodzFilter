# 0007 — Integrate sst-filters in the dsp layer

**Phase**: 2
**Layer**: dsp, build
**Depends on**: 0006

## Goal

The `dsp` library can run any candidate filter model of sst-filters (GPL v3, pinned commit, `filters++` API) on linked stereo, through a small processor `dsp::MultimodeFilter`: `prepare`, `reset`, `configure` (model setup, off the audio thread), `setCutoffHz`, `setResonance`, `process`. It processes in fixed internal packets of 16 samples kept across host blocks, so the result does not depend on the host block size. No parameter, no plugin wiring yet.

## Candidate models (for task 0008, listening)

The SPEC targets a clean multimode, a ladder, an OB-X style model and a diode. Candidates, all low pass for now: `CytomicSVF` (clean, 12 dB), `VemberClassic` 24 dB, `VintageLadder`, `OBXD_4Pole` 24 dB, `OBXD_2Pole` 12 dB, `DiodeLadder`, `K35`. The exact `ModelConfig` of each is listed in `dsp/FilterCandidates.h`.

## Expected behaviour (measurable)

| Test signal | Parameters | Expected result | Tolerance | Source |
| --- | --- | --- | --- | --- |
| Sine 100 Hz, peak 0.25, 48 kHz, steady state (after 4 800 samples) | `CytomicSVF` LP, cutoff 1 kHz, resonance 0 | output RMS / input RMS within ± 1 dB of 0 dB | ± 1 dB | 2-pole low pass, one decade below cutoff: \|H\| ≈ 1 |
| Sine 10 kHz, same | same | attenuation between 34 and 46 dB | ± 6 dB around −40 dB | 2-pole low pass falls 12 dB/octave: (1 kHz / 10 kHz)² = −40 dB |
| Sine 10 kHz / 100 Hz | `CytomicSVF` HP, cutoff 1 kHz, resonance 0 | 10 kHz within ± 1 dB of 0 dB; 100 Hz attenuated 34 to 46 dB | as above | mirror of the low pass |
| 1 s of pink noise, every candidate | cutoff 20 Hz, 1 kHz, 20 kHz × resonance 0, 0.5, 1 | every sample finite, peak below 4 (+24 dBFS for a 0.25 input) | exact finiteness | `.claude/rules/dsp.md`, finite output |
| Same 8 192 samples of noise, every candidate | cutoff 300 Hz then 5 kHz at sample 4 096; blocks of 1, 7, 64, 480, 512 | identical outputs | ± 1e-6 | block size independence (internal packets of 16 samples) |
| Render, `reset()`, render again | any candidate | identical | exact | `reset()` clears the state |
| Block of 0 samples | any | no crash, packet position unchanged | — | `.claude/rules/realtime-audio.md` |
| Stereo, same input on both channels | any candidate | left = right | exact | linked stereo (SPEC) |
| RTSan build | `process` marked `MOODZ_NONBLOCKING` | no report | — | `.claude/rules/realtime-audio.md` |

## Out of scope

* Choosing the 4 models (task 0008, by listening, ADR).
* Parameters, smoothing of cutoff and resonance beyond the per-packet coefficient interpolation of sst-filters, plugin wiring (task 0009).
* Changing the model while audio runs (needs a crossfade; later task).
* OB-Xf's ZDF filter (considered in 0008 if the sst candidates do not cover the OB-X target).
* Drive, oversampling (later phase 2 tasks).

## Reuse search (`.claude/rules/scope.md` rule 8)

Playbook fiche `docs/references/surge-and-sst.md` §1.2: sst-filters has 41 modelled filters, header only, without JUCE, GPL v3 (AGPL compatible); the `filters++` API takes caller-owned memory for delay lines. JUCE's `dsp` filters cannot be used in `dsp/` (no JUCE). Decision recorded as MoodzFilter ADR 0005 (GPL library dependency).

## Rules to load

* `.claude/rules/dsp.md`, `realtime-audio.md`, `cpp.md`, `testing.md`, `build.md`, `workflow.md`

## Definition of done

* [ ] sst-basic-blocks and sst-filters pulled by CPM at pinned commits, their headers as system includes (no warning from them under `-Werror`).
* [ ] MoodzFilter ADR 0005 (playbook) records the GPL v3 dependency.
* [ ] Tests written first, seen red, then green.
* [ ] Debug, Release, RTSan builds: 0 warnings, `ctest` green, `check` and `compare` unchanged (no plugin change).
* [ ] GLOSSARY: Filter model, Passband, Internal packet (check before the verifier).
* [ ] `rt-safety-reviewer` passed, CI green.
* [ ] `task-verifier`: pass.

## End note (filled by the task-verifier)
