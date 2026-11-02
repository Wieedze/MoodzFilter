# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 2 (DSP core: filters) |
| Gate to pass | Gate 2: DSP tests and golden references green, CPU budget set in `benchmarks/budget.json` and met, listening approved (playbook `docs/06_START_PLAN.md`, ROADMAP p2). Gate 1 passed 2026-11-02: green CI including RealtimeSanitizer (run 41524747297), green pluginval 10, Maxime's Live test at 22:16 (plugin loaded, Gain out from `-inf` to `+6.0 dB`, session saved and reloaded with the value kept) |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | None in progress. 0007 Integrate sst-filters in the dsp layer (ADR 0005, `dsp::MultimodeFilter`, 7 low pass candidates in `dsp/FilterCandidates.h`): done 2026-11-02, task-verifier pass (2 rounds). 0006 Gain out −∞ to +6 dB and JUCE generic editor (ADR 0004, `tools snapshot`): done 2026-11-02, task-verifier pass (1 round). 0005 Green CI (RealtimeSanitizer job, `MOODZ_NONBLOCKING`, strict C++23): done 2026-11-02, task-verifier pass (2 rounds). 0004 tools `render`, `compare`, `check` and first golden references: done 2026-11-02, task-verifier pass (2 rounds). 0003 Smoothed gain in TDD: done 2026-11-02, task-verifier pass (2 rounds). 0002 Separate the `dsp`, `plugin`, `ui`, `tools` targets: done 2026-11-02, task-verifier pass (2 rounds). 0001 Initialise the repo from Pamplejuce: done 2026-11-02, task-verifier pass |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | — |
| Next tasks | 0008: listening renders of the 7 filter candidates and Maxime's choice of the 4 models, recorded by ADR. 0009: plugin parameters Model, Mode, Cutoff, Resonance. Then the rest of the ROADMAP p2 lane: input drive with 2x and 4x oversampling (latency reported, dry path delayed); BPM-synced LFO on the cutoff (block size test with the LFO active); dry/wet mix, in/out gains, smoothing everywhere (Gain in −∞ to +12 dB, ADR 0004); CPU budget and listening in Live (gate 2). Each needs its task file before it starts. Known limit: sst-basic-blocks builds on x86_64 only without simde (ADR 0005) |
