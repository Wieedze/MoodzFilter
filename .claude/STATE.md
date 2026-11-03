# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 2 (DSP core: filters) |
| Gate to pass | Gate 2: DSP tests and golden references green, CPU budget set in `benchmarks/budget.json` and met, listening approved (playbook `docs/06_START_PLAN.md`, ROADMAP p2). Gate 1 passed 2026-11-02: green CI including RealtimeSanitizer (run 41524747297), green pluginval 10, Maxime's Live test at 22:16 (plugin loaded, Gain out from `-inf` to `+6.0 dB`, session saved and reloaded with the value kept) |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | None in progress. 0009 Filter list: five models with their passbands (`docs/tasks/0009-five-models-mode.md`, playbook ADRs 0006 and 0007): done 2026-11-03, task-verifier pass (1 round), PR #12. The Model parameter lists 9 entries in a frozen order (Cytomic SVF Low-pass, High-pass, Band-pass, Notch; Vintage Ladder Low-pass; OB-Xd 4-pole Low-pass; Diode Ladder Low-pass; K35 Low-pass, High-pass), `dsp::filterSetups()`. The task was rescoped mid-way: a first build with a Mode parameter and a low-pass fallback (8c85445, ADR 0006) was conforming in Live, then Maxime chose to merge model and passband (ADR 0007) so a model shows only what it has; `filterMode` was removed before any release. Maxime's Live test of the final build: 2026-11-03 14:29 CET, everything works. 0008 Audition build: done 2026-11-03 (PR #11). Earlier tasks 0001 to 0007: done 2026-11-02, see their task files and the playbook post-mortems |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | — |
| Next tasks | 0010 per-model settings (slope on OB-Xd 4-pole and Diode Ladder, Morph on OB-Xd 4-pole, K35 saturation, Vintage Ladder sub-model; ADR 0006). Its first decision (ADR 0007 consequences): do the settings join the Model list, which would multiply its entries (the OB-Xd 4-pole alone has 6 slopes), or stay separate parameters, and what happens on a model without the setting; decide with Maxime before the task file. Then the rest of the ROADMAP p2 lane: crossfade on a Model change (SPEC success criterion 3, not yet a ROADMAP item); input drive with 2x and 4x oversampling (latency reported, dry path delayed); BPM-synced LFO on the cutoff (block size test with the LFO active); dry/wet mix, Gain in (−∞ to +12 dB, ADR 0004), smoothing everywhere; CPU budget and listening in Live (gate 2). Known limit: sst-basic-blocks builds on x86_64 only without simde (ADR 0005) |

## Working environment

From 2026-11-03 Maxime works on his own Windows machine. Claude Code runs there inside WSL2 (Ubuntu, x86_64): the Linux build is for development and tests, and the VST3 tested in Live comes from the CI Windows installer (artifact `MoodzFilter-x.y.z-Windows.exe`; the browser blocks the unsigned download, so fetch it with `gh run download <run> -n MoodzFilter-x.y.z-Windows.exe -D /mnt/c/Users/Max/Downloads/...`). A native Windows build uses clang-cl and Ninja from a "x64 Native Tools Command Prompt for VS 2022", like the CI Windows job. WSL has no clang-20, pluginval or xvfb-run yet: the RealtimeSanitizer build and pluginval run in CI. Keep the playbook repo (VstKnowledgeBase-) cloned next to this one: the SPEC, ROADMAP, ADRs and post-mortems live there.
