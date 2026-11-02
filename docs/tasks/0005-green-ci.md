# 0005 — Green CI: RealtimeSanitizer, nonblocking annotations, strict C++23

**Phase**: 1
**Layer**: build, dsp, plugin (annotations only)
**Depends on**: 0004

## Goal

CI proves on every PR that nothing on the audio path allocates, locks or makes a blocking call: `processBlock` (now also `noexcept`, `.claude/rules/cpp.md`) and `dsp::SmoothedGain::process` are marked `[[clang::nonblocking]]`, a Linux job builds with RealtimeSanitizer (clang 20) and runs the tests and `MoodzFilterTools check` through them, and a canary proves the sanitizer really fires. Every target compiles as strict C++23 (no GNU extensions), including `PluginTests` and the plugin.

## Expected behaviour (measurable)

| Check | Expected result |
| --- | --- |
| `RtsanCanary` (built only with `-DWITH_REALTIME_SANITIZER=ON`): allocates inside a `nonblocking` function | RealtimeSanitizer stops it; ctest test `rtsan-canary` passes only if the output contains `RealtimeSanitizer: unsafe-library-call` (`PASS_REGULAR_EXPRESSION`). Seen failing (red) while `MOODZ_REALTIME_SANITIZER` was not yet defined, so the attribute was empty |
| RTSan build, `ctest` | all tests pass: `PluginTests` and `DspTests` call `processBlock` and `SmoothedGain::process` under the sanitizer with no report |
| RTSan build, `MoodzFilterTools check` | exit 0, no report (every render case through the real `processBlock`, blocks of 1 to 512 samples) |
| Normal builds (clang 18 here, clang-cl on Windows) | `MOODZ_NONBLOCKING` expands to nothing; 0 warnings |
| `ninja -t commands` on every target | never `-std=gnu++*`; our targets at `-std=c++23`. JUCE pins its own VST3 wrapper and binary-data objects to `-std=c++17` (still no extensions), which we leave as is |
| CI | new job "Linux RealtimeSanitizer" green next to Linux and Windows |

## Out of scope

* clang-format check in CI and the formatting of template files (separate formatting task).
* Benchmark budget in CI.
* Aligning `.claude/rules/build.md` (still lists macOS in CI) with plugin ADR 0002: rule change procedure, separate task.
* Any change of behaviour in `dsp/` or `plugin/`.

## Rules to load

* `.claude/rules/realtime-audio.md` (How to check it), `build.md`, `cpp.md`, `testing.md`, `workflow.md`

## Definition of done

* [x] Canary seen red (no sanitizer: exits 0, so `WILL_FAIL` fails), then green under RTSan.
* [x] Annotations on `processBlock` and `SmoothedGain::process` through a single build-configuration macro.
* [x] Local RTSan build (clang 20): `ctest` green, `MoodzFilterTools check` exit 0, no RTSan report.
* [x] All targets strict C++23 (no gnu++; JUCE wrapper objects at c++17).
* [x] Normal Debug and Release builds: 0 warnings, `ctest` green, pluginval 10 green.
* [x] `rt-safety-reviewer` passed (no blocking finding; canary regex and direct include applied in fa21551; two notes in FUTURE.md).
* [x] CI green on the three jobs (run 41517663478 on fa21551: Linux, Linux RealtimeSanitizer, Windows).
* [x] `task-verifier`: pass.

## End note (filled by the task-verifier)

Task-verifier pass on 2026-11-02, after 2 rounds. Round 1 failed on one item: the new term `Canary` (`RtsanCanary`, `rtsan-canary`) had no `GLOSSARY.md` entry. It was fixed in `528ad1f`, which also states `noexcept` on `processBlock` in the Goal. Checks re-run by the verifier:
* Debug clean rebuild: 0 warnings, ctest 22/22. Tools check, render and compare: exit 0.
* Release: 0 warnings, ctest 22/22.
* pluginval 10: SUCCESS on Debug and Release.
* RTSan build (clang 20): ctest 23/23 including `rtsan-canary`. Tools check: exit 0.
* Canary: red without `MOODZ_REALTIME_SANITIZER`, green with it.
* An allocation injected into `processBlock` in a scratch copy was caught by RTSan in PluginTests.
* No `-std=gnu` anywhere.
* No new clang-format violation, and `tests/golden/` is unchanged.

CI run 41517663478 on `fa21551` passed on all three jobs. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0005-green-ci.md`.
