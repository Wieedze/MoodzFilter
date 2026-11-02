# 0002 — Separate the dsp, plugin, ui and tools targets

**Phase**: 1
**Layer**: build
**Depends on**: 0001

## Goal

The code is split into the layers of the playbook architecture: a `dsp` static library that cannot see JUCE, the JUCE plugin layer in `source/plugin/`, the editor in `source/ui/`, a `MoodzFilterTools` command line target, and two test targets (`DspTests` without JUCE, `PluginTests` with it).

## Expected behaviour (measurable)

| Check | Expected result |
| --- | --- |
| `DspTests` compiled without JUCE on its include path | test `dsp layer / JUCE is not reachable` green: `__has_include(<juce_core/juce_core.h>)` is false |
| A `#include <juce_...>` added under `source/dsp/` | CMake configure fails with a message naming the file |
| `PluginTests` | the existing `Plugin instance` tests green, unchanged |
| `MoodzFilterTools --help` | exit code 0, prints the usage |
| `MoodzFilterTools` with an unknown command | exit code 2 |
| `MOODZ_BUILD_TOOLS` | ON by default, OFF by default when `CMAKE_BUILD_TYPE` is Release |
| Debug and Release builds (Linux, clang) | zero warnings; `dsp` built with `-Wall -Wextra -Werror` |
| pluginval 10 on the VST3 | `SUCCESS` |

## Out of scope

* Any DSP processor, parameter or `Params` snapshot (task 0003, smoothed gain).
* `tools render`, `check`, `compare`, golden references (task 0004).
* RealtimeSanitizer CI job (task "Green CI").
* Any change to the editor beyond moving it and `createEditor` to `source/ui/`.

## Rules to load

* `.claude/rules/build.md`, `.claude/rules/cpp.md`, `.claude/rules/dsp.md` (JUCE-free layer), `.claude/rules/testing.md`, `.claude/rules/workflow.md`

## Skills

* `tdd` (the guard test is seen red before the target separation)

## Definition of done

* [x] Guard test written first and seen red while `DspTests` still sees JUCE.
* [x] `dsp` STATIC library: glob of `source/dsp/`, C++23, PIC, `-Wall -Wextra -Werror` (`/W4 /WX` on MSVC), no fast math, no JUCE.
* [x] Configure-time guard against `#include <juce` in `source/dsp/`.
* [x] `source/plugin/` and `source/ui/` globbed into `SharedCode`, which links `dsp`; `createEditor` defined in `source/ui/`.
* [x] `DspTests` (links `dsp`, `Catch2::Catch2WithMain`) and `PluginTests` (via `pamplejuce_add_tests`) discovered by `ctest`.
* [x] `MoodzFilterTools` target behind `MOODZ_BUILD_TOOLS`.
* [x] `CLAUDE.md` build commands and repo map updated.
* [x] Builds with no warning, `ctest` green, pluginval 10 green, CI green (run 41477871641).
* [x] `task-verifier`: pass.

## End note (filled by the task-verifier)

Pass on 2026-11-02 after 2 rounds. Round 1 failed on `-std=gnu++23` for `DspTests` and `MoodzFilterTools` (`cpp.md`, no compiler extensions) and a clang-format include order in `benchmarks/Catch2Main.cpp`; both fixed in `93cec1a`. Checked: Debug clean build and Release incremental build with 0 warnings, `ctest` 3/3, guard test red reproduced on the JUCE include path, configure guard fails on a JUCE include under `source/dsp/`, `MoodzFilterTools` exit codes 0 and 2, `MOODZ_BUILD_TOOLS` ON in Debug and OFF in Release, pluginval 10 `SUCCESS`, CI run 41480714455 green on Linux and Windows. Post-mortem: playbook `docs/plugins/moodzfilter/learning/0002-separate-targets.md`.
