# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 2 (DSP core: filters) |
| Gate to pass | Gate 2: DSP tests and golden references green, CPU budget set in `benchmarks/budget.json` and met, listening approved (playbook `docs/06_START_PLAN.md`, ROADMAP p2). Gate 1 passed 2026-11-02: green CI including RealtimeSanitizer (run 41524747297), green pluginval 10, Maxime's Live test at 22:16 (plugin loaded, Gain out from `-inf` to `+6.0 dB`, session saved and reloaded with the value kept) |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | **0008 Audition build** (`docs/tasks/0008-audition-build.md`), in progress on branch `task/0008-audition-build`, PR #11 (not merged). Done: parameters Model (the 7 candidates), Cutoff, Resonance wired before Gain out; `golden:` commit ac02387; rt-safety review fixes f1fe643; verifier round 1 FAIL (4 items: version hint test, full model order test, resonance mapping test, dsp layer declared) all fixed in b0de2b0 (ctest 44/44 on Debug). CI run 41541498630 green on 93bf471 (its Windows installer is what Maxime tests). **Remaining, in order:** (1) Maxime tests the 7 models in Live and names his 4, and accepts the golden change (filter wide open in every render case); record both in the task file DoD; (2) rerun the task-verifier (round 2) on the branch head with green CI; (3) on PASS: post-mortem in the playbook, close, index-writer; (4) Maxime merges PR #11 only once announced "ready". Earlier tasks 0001 to 0007: done 2026-11-02, see their task files and the playbook post-mortems |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | — |
| Next tasks | 0009: keep Maxime's 4 models (ADR in the playbook choices/, SPEC "Model" list frozen from then), add Mode (passband per model), task file first. Then the rest of the ROADMAP p2 lane: crossfade on model change (SPEC success criterion 3); input drive with 2x and 4x oversampling (latency reported, dry path delayed); BPM-synced LFO on the cutoff (block size test with the LFO active); dry/wet mix, Gain in (−∞ to +12 dB, ADR 0004), smoothing everywhere; CPU budget and listening in Live (gate 2). Known limit: sst-basic-blocks builds on x86_64 only without simde (ADR 0005) |

## Working environment

From 2026-11-03 Maxime works on his own Windows machine with VS Code and Claude Code, so builds are local and the VST3 is tested directly in Live. Build on Windows with clang-cl and Ninja from a "x64 Native Tools Command Prompt for VS 2022", like the CI Windows job. The RealtimeSanitizer job stays Linux-only, in CI. Keep the playbook repo (VstKnowledgeBase-) cloned next to this one: the SPEC, ROADMAP, ADRs and post-mortems live there.
