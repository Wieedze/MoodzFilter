# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 1 (skeleton) |
| Gate to pass | Gate 1: green CI, green pluginval 10, VST3 loaded in Ableton Live (Windows), `STATE.md` up to date |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | 0004 `tools render` and `compare` with the first golden reference (next). 0003 Smoothed gain in TDD: done 2026-11-02, task-verifier pass (2 rounds). 0002 Separate the `dsp`, `plugin`, `ui`, `tools` targets: done 2026-11-02, task-verifier pass (2 rounds). 0001 Initialise the repo from Pamplejuce: done 2026-11-02, task-verifier pass |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | — |
| Next tasks | 0004 `tools render` and `compare` with the first golden reference. Then: Green CI, which includes `[[clang::nonblocking]]` on `processBlock` and the DSP `process` functions and the Linux RealtimeSanitizer job (clang ≥ 20; deferred from 0003); load the VST3 in Ableton Live (gate) |
