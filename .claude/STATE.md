# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 2 (DSP core: filters) |
| Gate to pass | Gate 2: DSP tests and golden references green, CPU budget set in `benchmarks/budget.json` and met, listening approved (playbook `docs/06_START_PLAN.md`, ROADMAP p2). Gate 1 passed 2026-11-02: green CI including RealtimeSanitizer (run 41524747297), green pluginval 10, Maxime's Live test at 22:16 (plugin loaded, Gain out from `-inf` to `+6.0 dB`, session saved and reloaded with the value kept) |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | 0009 Freeze the five filter models and add Mode (`docs/tasks/0009-five-models-mode.md`, playbook ADR 0006): in progress on branch `task/0009-five-models-mode`. Task file written (commit a4cbf41, pushed, no PR yet); nothing coded yet. Next step: TDD, starting with the frozen parameter list `{ gainOut, filterModel, cutoff, resonance, filterMode }` and the 5-model `filterModel` order of ADR 0006. Previous: 0008 Audition build done 2026-11-03, task-verifier pass (2 rounds), PR #11 merged into main; Maxime kept 5 of the 7 candidates and approved the golden change ac02387. Tasks 0001 to 0007: done 2026-11-02, see their task files and the playbook post-mortems |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | — |
| Next tasks | After 0009: 0010 per-model settings (slope on OB-Xd 4-pole and Diode Ladder, Morph on OB-Xd 4-pole, K35 saturation, Vintage Ladder sub-model; parameters decided by listening, ADR 0006). Then the rest of the ROADMAP p2 lane: input drive with 2x and 4x oversampling (latency reported, dry path delayed); BPM-synced LFO on the cutoff (block size test with the LFO active); dry/wet mix, Gain in (−∞ to +12 dB, ADR 0004), smoothing everywhere; CPU budget and listening in Live (gate 2). Not yet a ROADMAP item: crossfade on model or Mode change (SPEC success criterion 3, out of scope of 0009). Model blending and a random cutoff curve stay out of v1 (`FUTURE.md`). Known limit: sst-basic-blocks builds on x86_64 only without simde (ADR 0005) |

## Working environment

From 2026-11-03 Maxime works on his own Windows machine. Claude Code runs there inside WSL2 (Ubuntu, x86_64): the Linux build is for development and tests, and the VST3 tested in Live comes from the CI Windows installer (artifact `MoodzFilter-x.y.z-Windows.exe`; the browser blocks the unsigned download, so fetch it with `gh run download <run> -n MoodzFilter-x.y.z-Windows.exe -D /mnt/c/Users/Max/Downloads/...`). A native Windows build uses clang-cl and Ninja from a "x64 Native Tools Command Prompt for VS 2022", like the CI Windows job. WSL has no clang-20, pluginval or xvfb-run yet: the RealtimeSanitizer build and pluginval run in CI. Keep the playbook repo (VstKnowledgeBase-) cloned next to this one: the SPEC, ROADMAP, ADRs and post-mortems live there.
