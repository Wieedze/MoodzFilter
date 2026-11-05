# STATE

Maintained by hand. A task does not close without updating this file.

| Field | Value |
| --- | --- |
| Plugin | MoodzFilter by Radikal Moodz (SPEC: playbook `docs/plugins/moodzfilter/SPEC.md`) |
| Current phase | 2 (DSP core: filters) |
| Gate to pass | Gate 2: DSP tests and golden references green, CPU budget set in `benchmarks/budget.json` and met, listening approved (playbook `docs/06_START_PLAN.md`, ROADMAP p2). Gate 1 passed 2026-11-02: green CI including RealtimeSanitizer (run 41524747297), green pluginval 10, Maxime's Live test at 22:16 (plugin loaded, Gain out from `-inf` to `+6.0 dB`, session saved and reloaded with the value kept) |
| Plugin identity (ADR 0002) | `Rdkm` / `Mzf1`, VST3 only, Windows only for v1. Final once a session is saved in Live |
| Current task | — (next: a generic editor without the scroll, decided by Maxime on 2026-11-05, to write). 0015 Synced LFO: done 2026-11-05 (PR #18): LFO shape, rate and depth on the cutoff, locked to the song position. 0014 Bad input samples: done 2026-11-05 (PR #17): a sample beyond ±256, NaN and infinities included, counts as 0. 0013 Input drive: done 2026-11-05 (PR #16): Drive 0 to 24 dB and Drive curve inside the oversampling, transparent at 0 dB, no golden change. 0012 Oversampling: done 2026-11-05 (PR #15): Off, 2x, 4x around the filter (default 2x), 40 samples of constant latency, bypass delayed by it, a change crossfades with a pre-roll. 0011 Crossfade between filter variants: done 2026-11-03 (PR #14): a Model change, a Diode slope step or a Vintage variant change crossfades over 20 ms. 0010 Per-model settings: done 2026-11-03 (PR #13, merged). 0009 (PR #12) and 0008 (PR #11): done 2026-11-03. Earlier tasks 0001 to 0007: done 2026-11-02, see their task files and the playbook post-mortems |
| Pinned JUCE version | 9.0.5 |
| CPU budget per block | — |
| Known defects (sound, perf, hosts) | Asymmetric at high Drive leaves a low tail when a signal stops abruptly (`FUTURE.md`), not yet judged by listening |
| Next tasks | A generic editor that shows every parameter without scrolling (`FUTURE.md`); then the rest of the ROADMAP p2 lane: dry/wet mix with a dry path delayed by the constant latency, Gain in (−∞ to +12 dB, ADR 0004), smoothing everywhere; CPU budget and listening in Live (gate 2: benchmark the chain with drive and LFO, `benchmarks/Benchmarks.cpp` covers only the boot; includes the cost of 4x and of two chains during a crossfade). An LFO matrix is requested for after v1, a SPEC change (`FUTURE.md`). Known limit: sst-basic-blocks builds on x86_64 only without simde (ADR 0005) |

## Working environment

From 2026-11-03 Maxime works on his own Windows machine. Claude Code runs there inside WSL2 (Ubuntu, x86_64): the Linux build is for development and tests, and the VST3 tested in Live comes from the CI Windows installer (artifact `MoodzFilter-x.y.z-Windows.exe`; the browser blocks the unsigned download, so fetch it with `gh run download <run> -n MoodzFilter-x.y.z-Windows.exe -D /mnt/c/Users/Max/Downloads/...`). A native Windows build uses clang-cl and Ninja from a "x64 Native Tools Command Prompt for VS 2022", like the CI Windows job. WSL has no clang-20, pluginval or xvfb-run yet: the RealtimeSanitizer build and pluginval run in CI. Keep the playbook repo (VstKnowledgeBase-) cloned next to this one: the SPEC, ROADMAP, ADRs and post-mortems live there.
