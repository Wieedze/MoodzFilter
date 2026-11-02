# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 1 (skeleton) |
| Gate to pass | Gate 1: green CI, green pluginval 10, VST3 loaded in Ableton Live (Windows), `STATE.md` up to date |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | Gate 1 check (next): Maxime loads the VST3 in Ableton Live (Windows) from the Windows installer CI artefact, saves a session and reloads it. 0005 Green CI (RealtimeSanitizer job, `MOODZ_NONBLOCKING` on `processBlock` and `SmoothedGain::process`, strict C++23): done 2026-11-02, task-verifier pass (2 rounds). 0004 tools `render`, `compare`, `check` and first golden references: done 2026-11-02, task-verifier pass (2 rounds). 0003 Smoothed gain in TDD: done 2026-11-02, task-verifier pass (2 rounds). 0002 Separate the `dsp`, `plugin`, `ui`, `tools` targets: done 2026-11-02, task-verifier pass (2 rounds). 0001 Initialise the repo from Pamplejuce: done 2026-11-02, task-verifier pass |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | — |
| Next tasks | 0006: Gain out range −∞ to +6 dB (playbook MoodzFilter ADR 0004, SPEC amended 2026-11-02; golden `gain-automation` will change, listening needed) and the JUCE generic editor (SPEC interface, one control per parameter), both asked by Maxime. Gate 1 status: Maxime loaded the VST3 in Ableton Live on 2026-11-02 from the CI installer and moved Gain out (works); still to confirm: save a session and reload it with the value kept, ideally on the 0006 build |
