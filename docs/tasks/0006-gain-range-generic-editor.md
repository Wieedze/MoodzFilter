# 0006 — Gain out from −∞ to +6 dB, JUCE generic editor

**Phase**: 1
**Layer**: dsp, plugin, ui, tools
**Depends on**: 0005

## Goal

Gain out follows the amended SPEC (playbook MoodzFilter ADR 0004): from silence to +6 dB, the lowest position displayed `-inf` and giving a gain of exactly 0. The template's "Hello" screen is replaced by the JUCE generic editor (SPEC: interface), one control per parameter. `MoodzFilterTools snapshot` writes a PNG of the editor for review (`.claude/rules/ui.md`).

## Expected behaviour (measurable)

| Test signal | Parameters | Expected result | Tolerance | Source |
| --- | --- | --- | --- | --- |
| DC at 1.0, 48 kHz | target +60 dB | clamped to +6 dB: 10^(6/20) = 1.9952623 | ± 1e-5 | ADR 0004, Python `10**(6/20)` |
| DC at 1.0 | target −60 dB (the silence floor) or lower, e.g. −100 dB | after the 20 ms ramp, every sample exactly 0 | exact (0.0f) | ADR 0004 |
| DC at 1.0 | target −59.9 dB (just above the floor) | 10^(−59.9/20) = 0.00101158, not 0 | ± 1e-7 | Python `10**(-59.9/20)` |
| DC at 1.0 | 0 dB, then the floor | ramp from 1 to 0 in 960 samples, never a step above 1/960 = 0.0010417 | ± 1e-6 | linear ramp of 20 ms |
| Earlier DSP tests (+6 dB, −24 dB, ramp, NaN, block sizes, 0 samples, linked channels, invalid rate) | unchanged, except targets above +6 dB brought to +6 dB | still green | as before | task 0003 |
| `gainOut` parameter | — | range −60 to +6 dB, default 0 dB, same ID `gainOut` (ID list frozen test unchanged) | ± 1e-6 | ADR 0004, `.claude/rules/juce.md` |
| `gainOut` text | value −60 dB / 0 dB / +6 dB | `-inf` / `0.0 dB` / `+6.0 dB` | exact strings | ADR 0004 |
| `gainOut` text to value | `-inf` | −60 dB | ± 1e-6 | — |
| Editor | `createEditor()` | a `juce::GenericAudioProcessorEditor` | — | SPEC interface |
| `MoodzFilterTools snapshot <png>` | — | exit 0, PNG written, width and height > 0; run in CI and attached to the renders artefact | — | `.claude/rules/ui.md` |
| Render cases | `gain-automation` −12 dB then +6 dB; new `gain-mute` 0 dB then silence | new references through a `golden:` commit, approved by listening | manifest tolerances | ADR 0003, ADR 0004 |

## Out of scope

* Gain in, and any other parameter.
* Any custom look: the generic editor as JUCE draws it. Scaling at 150 % and 200 % (`.claude/rules/ui.md`) comes with a custom editor, out of v1 (SPEC).
* Presets.
* Removing the melatonin_inspector module (still linked in Debug, unused): separate cleanup.

## Decision notes

* The silence floor is −60 dB internally: at −60 dB and below, the gain is exactly 0. Values in a saved state are in dB, so a session saved with Gain out above +6 dB loads clamped to +6 dB. No release exists, so no state version bump.
* Skew of the control: centre at −12 dB, so that the useful range (−24 to +6 dB) takes most of the travel. Taste, adjustable after listening.

## Rules to load

* `.claude/rules/dsp.md`, `realtime-audio.md`, `juce.md`, `ui.md`, `testing.md`, `cpp.md`, `workflow.md`

## Definition of done

* [x] Tests changed and added first, seen red (DSP 2 of 9 failing, plugin 3 of 4 failing).
* [x] Debug, Release and RTSan builds: 0 warnings, `ctest` green (25, 25, 26 with the canary), `check` green.
* [x] Snapshot PNG produced and looked at (856 × 250, one "Gain out" slider at 0.0 dB; also attached to CI run 41524747297).
* [x] New references in a separate `golden:` commit (ff69806); **Maxime approved them by listening on 2026-11-02 at 22:16 (CI artefact of run 41524747297).**
* [x] **Maxime, 2026-11-02 22:16, Ableton Live on Windows with the installer of run 41524747297: generic editor shown, Gain out from `-inf` (silence) to `+6.0 dB`, saved session reloaded with the value kept ("tout est parfait").**
* [x] GLOSSARY up to date (Generic editor, Silence floor).
* [x] pluginval 10 green, `rt-safety-reviewer` passed (no finding; non-finite input guard noted in FUTURE.md), CI green (run 41524747297 on ae7f615: Linux, Linux RealtimeSanitizer, Windows).
* [x] `task-verifier`: pass.

## End note (filled by the task-verifier)

Pass on 2026-11-02, first round. Debug and Release: 0 warnings, `ctest` 25/25; RTSan: `ctest` 26/26 with the canary, `check` green; `check` and `compare` green (16 renders, max error 0); pluginval 10 `SUCCESS`; snapshot 856 × 250 looked at; only `ff69806` touches `tests/golden/`; CI run 41524747297 green on Linux, Linux RealtimeSanitizer and Windows. Every Expected behaviour row is covered with its tolerance. Files outside the announced layers (CI workflow, `benchmarks/Catch2Main.cpp`, the `MOODZ_WITH_INSPECTOR` define) follow from the snapshot row and the deleted template editor. Gate 1 is passed with Maxime's Live test of 22:16. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0006-gain-range-generic-editor.md`.
