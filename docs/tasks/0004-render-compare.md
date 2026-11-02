# 0004 — tools render, compare and check, first golden references

**Phase**: 1
**Layer**: tools, build
**Depends on**: 0003

## Goal

`MoodzFilterTools` drives the real `PluginProcessor` headless: `render` writes WAV files for a fixed set of cases and test signals, `compare` checks them against `tests/golden/` with tolerances from `tests/golden/manifest.json`, `check` runs a fast battery (finite output, block size independence through the whole plugin). CI runs the three on Linux and Windows. The first references are committed in a separate `golden:` commit and validated by Maxime by listening.

## Expected behaviour (measurable)

| Check | Expected result | Tolerance | Source |
| --- | --- | --- | --- |
| Test signals (no JUCE, `ToolsTests`) | impulse: 1 sample at 0.25 then zeros; sine 1 kHz peak 0.25 at 48 kHz: sample 12 = 0.25; exponential sweep 20 Hz to 20 kHz: starts at 0, peak ≤ 0.25, mean frequency over the last 256 samples within 3 % of 20 kHz by zero crossing count (the sweep rises about 3.7 % across those samples); pink noise: deterministic (same seed, same samples), peak ≤ 0.25, power spectral density slope between −2 and −4 dB per octave (pink: −3), Welch average of 1 024-sample Hann frames, octave bands 125 Hz to 8 kHz | as stated | Farina 2000 (sweep), Paul Kellet's pink filter (economy version) |
| Comparison metrics (no JUCE, `ToolsTests`) | identical buffers: max error 0, RMS error −∞ dBFS reported as ≤ −300; a difference of 0.001 on every sample: max 0.001, RMS −60 dBFS | ± 1e-9, ± 0.01 dB | 20·log10(0.001) = −60 |
| `render <dir>` | writes `<case>__<signal>.wav` for 3 cases × 4 signals, stereo, 48 kHz, 32-bit float | exact file list | — |
| `compare <renders> <golden>` on identical folders | exit 0, one line per file with max error and RMS error | — | — |
| `compare` with one sample changed by 0.01 in a render | exit 1, names the file | — | — |
| `compare` with a render missing, or a render not listed in the manifest | exit 1, names the file | — | — |
| `check` | exit 0: every case × signal finite, and rendered with blocks of 1, 64 and 480 samples identical to the 512-sample render | ± 1e-7 | block size independence, `.claude/rules/dsp.md` |
| CI (Linux, Windows) | `check`, `render`, `compare` steps green; renders uploaded as an artefact on Linux | — | — |

## Render cases (in code, `renderCases()` in `tools/Rendering.cpp`)

| Case | Gain out |
| --- | --- |
| `unity` | 0 dB |
| `gain-plus6` | +6 dB |
| `gain-automation` | −12 dB, then +12 dB from the middle of the signal (exercises the 20 ms ramp) |

Signals, all at peak 0.25 (−12 dBFS), both channels identical: `impulse` (12 288 samples), `sine-1k` (12 288), `sweep` (49 152), `pink-noise` (24 576).

## Out of scope

* Presets (`--preset all` comes with the presets task); PNG spectrograms; `bench`; `snapshot`.
* RealtimeSanitizer job and `[[clang::nonblocking]]` (Green CI task).
* Any change to `dsp/` or `plugin/`.

## Reuse search (`.claude/rules/scope.md` rule 8)

WAV reading and writing and JSON: JUCE (`juce_audio_formats`, `juce::JSON`), first in the selection order. Test signals and error metrics are a few lines each; no library worth a dependency.

## Decision to record

Format of `tests/golden/manifest.json` (open question of the playbook): plugin ADR 0003.

## Rules to load

* `.claude/rules/testing.md` (golden references), `build.md`, `cpp.md`, `workflow.md`

## Definition of done

* [x] Signal and metric tests written first, seen red (ToolsTests against stubs: 5 of 5 failed).
* [x] `render`, `compare`, `check` behave as in the table (commands and exit codes in PR #5).
* [x] Manifest format recorded in plugin ADR 0003 (playbook `docs/plugins/moodzfilter/choices/0003-golden-manifest-format.md`).
* [x] References in `tests/golden/` added by a separate `golden:` commit (4a54fcf).
* [x] **Maxime listened to the references (CI artefact of run 41507926670) and approved them on 2026-11-02 at 20:23 in the session: gain louder on `gain-plus6`, unchanged on `unity`, automation quiet then loud with no click; impulses inaudible as expected.**
* [x] CI runs `check`, `render`, `compare` on Linux and Windows, green (run 41507926670).
* [ ] Builds with no new warning, `ctest` green, pluginval 10 green.
* [ ] `task-verifier`: pass.

## End note (filled by the task-verifier)
