# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 2 (DSP core: filters) |
| Gate to pass | Gate 2: DSP tests and golden references green, CPU budget set in `benchmarks/budget.json` and met, listening approved (playbook `docs/06_START_PLAN.md`, ROADMAP p2). Gate 1 passed 2026-11-02: green CI including RealtimeSanitizer (run 41524747297), green pluginval 10, Maxime's Live test at 22:16 (plugin loaded, Gain out from `-inf` to `+6.0 dB`, session saved and reloaded with the value kept) |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | None in progress. 0010 Per-model settings (`docs/tasks/0010-per-model-settings.md`, playbook ADR 0008): done 2026-11-03, task-verifier pass (1 round), PR #13. Three parameters: Slope 6 to 24 dB/oct (OB-Xd 4-pole continuously through Morph, Diode Ladder at the nearest 6 dB step), K35 saturation 0 to 100 % (25 % = Mild), Vintage variant (6 sub-models); 17 prepared filter variants, `dsp::chooseFilter` picks one per block; defaults give back the task 0009 sound exactly. Maxime's Live test: 2026-11-03 16:32 CET, approved. 0009 Filter list (PR #12) and 0008 Audition build (PR #11): done 2026-11-03. Earlier tasks 0001 to 0007: done 2026-11-02, see their task files and the playbook post-mortems |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | — |
| Next tasks | Crossfade on a Model change (SPEC success criterion 3; also covers a Diode slope step and a Vintage variant change, and the slope hysteresis in `FUTURE.md`). Then the rest of the ROADMAP p2 lane: input drive with 2x and 4x oversampling (latency reported, dry path delayed); BPM-synced LFO on the cutoff (block size test with the LFO active); dry/wet mix, Gain in (−∞ to +12 dB, ADR 0004), smoothing everywhere; CPU budget and listening in Live (gate 2). Known limit: sst-basic-blocks builds on x86_64 only without simde (ADR 0005) |

## Working environment

From 2026-11-03 Maxime works on his own Windows machine. Claude Code runs there inside WSL2 (Ubuntu, x86_64): the Linux build is for development and tests, and the VST3 tested in Live comes from the CI Windows installer (artifact `MoodzFilter-x.y.z-Windows.exe`; the browser blocks the unsigned download, so fetch it with `gh run download <run> -n MoodzFilter-x.y.z-Windows.exe -D /mnt/c/Users/Max/Downloads/...`). A native Windows build uses clang-cl and Ninja from a "x64 Native Tools Command Prompt for VS 2022", like the CI Windows job. WSL has no clang-20, pluginval or xvfb-run yet: the RealtimeSanitizer build and pluginval run in CI. Keep the playbook repo (VstKnowledgeBase-) cloned next to this one: the SPEC, ROADMAP, ADRs and post-mortems live there.
