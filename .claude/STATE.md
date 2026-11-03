# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 2 (DSP core: filters) |
| Gate to pass | Gate 2: DSP tests and golden references green, CPU budget set in `benchmarks/budget.json` and met, listening approved (playbook `docs/06_START_PLAN.md`, ROADMAP p2). Gate 1 passed 2026-11-02: green CI including RealtimeSanitizer (run 41524747297), green pluginval 10, Maxime's Live test at 22:16 (plugin loaded, Gain out from `-inf` to `+6.0 dB`, session saved and reloaded with the value kept) |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | None in progress. 0008 Audition build (`filterModel` with the 7 candidates, `cutoff`, `resonance`, before Gain out): done 2026-11-03, task-verifier pass (2 rounds), PR #11. Maxime's Live test 2026-11-03 09:43 CET: all 7 models work; he kept 5 (Diode Ladder 24 dB, K35, Vintage Ladder, Cytomic SVF, OB-Xd 4-pole 24 dB) and approved the golden change ac02387. Earlier tasks 0001 to 0007: done 2026-11-02, see their task files and the playbook post-mortems |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | — |
| Next tasks | 0009: keep Maxime's 5 models. Starts with a SPEC decision, because the SPEC says 4 models and lists "More than 4 filter models" in Out of v1: grill, ADR in the playbook choices/, amended SPEC, then the Model list frozen. Also in the same discussion: Mode (passband per model), and Maxime's requests in `FUTURE.md` (user-selectable slope per model, blend of models, random curve on the cutoff). Task file first; rename the ROADMAP item "Plugin parameters Model, Mode, Cutoff, Resonance" when it starts. Then the rest of the ROADMAP p2 lane: crossfade on model change (SPEC success criterion 3); input drive with 2x and 4x oversampling (latency reported, dry path delayed); BPM-synced LFO on the cutoff (block size test with the LFO active); dry/wet mix, Gain in (−∞ to +12 dB, ADR 0004), smoothing everywhere; CPU budget and listening in Live (gate 2). Known limit: sst-basic-blocks builds on x86_64 only without simde (ADR 0005) |

## Working environment

From 2026-11-03 Maxime works on his own Windows machine. Claude Code runs there inside WSL2 (Ubuntu, x86_64): the Linux build is for development and tests, and the VST3 tested in Live comes from the CI Windows installer (artifact `MoodzFilter-x.y.z-Windows.exe`; the browser blocks the unsigned download, so fetch it with `gh run download <run> -n MoodzFilter-x.y.z-Windows.exe -D /mnt/c/Users/Max/Downloads/...`). A native Windows build uses clang-cl and Ninja from a "x64 Native Tools Command Prompt for VS 2022", like the CI Windows job. WSL has no clang-20, pluginval or xvfb-run yet: the RealtimeSanitizer build and pluginval run in CI. Keep the playbook repo (VstKnowledgeBase-) cloned next to this one: the SPEC, ROADMAP, ADRs and post-mortems live there.
