# CLAUDE.md — Router

MoodzFilter: a multi-model filter effect, VST3, Windows, AGPLv3. Built with the playbook https://github.com/Wieedze/VstKnowledgeBase-.

This file routes to the right rules, docs and decisions for the task. **It contains no rules.** Rules live in `.claude/rules/`, task files in `docs/tasks/`.

## Where the plugin's documents live

The SPEC, the ROADMAP, the plugin ADRs and the post-mortems live in the playbook repo, under `docs/plugins/moodzfilter/`, because the index website (https://wieedze.github.io/VstKnowledgeBase-/) is built from there. This repo holds the code, the task files and `.claude/STATE.md`.

| Document | Location |
| --- | --- |
| SPEC (scope of v1, Out of v1) | playbook `docs/plugins/moodzfilter/SPEC.md` |
| ROADMAP (phases, gates, tasks) | playbook `docs/plugins/moodzfilter/ROADMAP.md` |
| Plugin ADRs | playbook `docs/plugins/moodzfilter/choices/` |
| Post-mortems | playbook `docs/plugins/moodzfilter/learning/` |
| Task files | this repo, `docs/tasks/NNNN-*.md` |
| Current state | this repo, `.claude/STATE.md` |

## Reading order at the start of every task

1. This file.
2. `.claude/STATE.md`: where the project stands, current task, known defects.
3. The task file in `docs/tasks/`. Read its `Out of scope` and `Definition of done` sections.
4. The SPEC (playbook).
5. `GLOSSARY.md`: use these terms in code, tests and messages.
6. The rules in `.claude/rules/` that match the task (table below). Not all of them.
7. The plugin ADRs that touch the area, and post-mortems of similar tasks (playbook).

## Routing table

| If the task touches… | Rules | Skills | Review agent |
| --- | --- | --- | --- |
| Pure DSP (`dsp/`) | `realtime-audio.md`, `cpp.md`, `dsp.md`, `testing.md` | `tdd` | `rt-safety-reviewer` |
| JUCE plugin layer (`plugin/`): parameters, state, processBlock | `realtime-audio.md`, `cpp.md`, `juce.md`, `testing.md` | `tdd` | `rt-safety-reviewer` |
| UI (`ui/`) | `cpp.md`, `juce.md`, `ui.md` | — | — |
| CMake, CI, packaging | `build.md` | — | — |
| New feature or scope change | `scope.md` | `grill-with-docs`, `to-spec`, `to-tickets` | — |
| Bug | `testing.md` | `diagnosing-bugs` | — |
| Any task, at the end | `workflow.md` | `code-review` | `task-verifier` (**mandatory**) |

## Mandatory end of task

Every task ends with the `task-verifier` agent, then the `index-writer` agent. Details: `.claude/rules/workflow.md`.

## Hard constraints (the only ones that live here)

* **Nothing that allocates, locks, does I/O or can throw an exception on the audio thread.** Details: `.claude/rules/realtime-audio.md`.
* **No DSP code without a test that comes before it.** Details: `.claude/rules/testing.md`.
* **No silent scope extension.** Whatever is not in the task goes into `FUTURE.md`. Details: `.claude/rules/scope.md`.
* **No parameter ID renamed or removed after a release**: it breaks users' sessions. Details: `.claude/rules/juce.md`.
* **No plugin identity code changed** (`Rdkm`, `Mzf1`) once a session is saved in Live. Plugin ADR 0002.
* **No code copied from an AGPL/GPL project** without an explicit decision recorded in an ADR.

## Build commands

```bash
# Configure (once, or after CMakeLists.txt changes). Debug by default.
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build Builds

# Tests (DspTests: dsp layer without JUCE; PluginTests: plugin layer)
ctest --test-dir Builds --output-on-failure
./Builds/DspTests "[tag]"
./Builds/PluginTests "[test name]"

# Command line tools (ON by default, OFF in Release; -DMOODZ_BUILD_TOOLS=ON to force)
./Builds/MoodzFilterTools check                                  # finite output, block size independence
./Builds/MoodzFilterTools render Builds/renders                  # one WAV per case and signal
./Builds/MoodzFilterTools compare Builds/renders tests/golden    # against the references (plugin ADR 0003)
# Never refresh tests/golden/ to make compare pass: a reference changes only in a
# separate "golden: <reason>" commit, after Maxime listened to the new renders.

# Benchmarks
./Builds/Benchmarks

# pluginval (download v1.0.4 from github.com/Tracktion/pluginval/releases)
pluginval --strictness-level 10 --validate Builds/MoodzFilter_artefacts/Debug/VST3/MoodzFilter.vst3
```

Release: `-DCMAKE_BUILD_TYPE=Release` in a separate build folder. Use it to make music with the plugin; Debug is for development.

Linux build dependencies: `libasound2-dev libx11-dev libxi-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev libglu1-mesa-dev xvfb ninja-build clang`.

Sanitizers: `-DWITH_REALTIME_SANITIZER=ON` (clang ≥ 20, Linux), `-DWITH_ADDRESS_SANITIZER=ON`, `-DWITH_THREAD_SANITIZER=ON`.

RealtimeSanitizer build, as in the CI job (executables only: the VST3 cannot link the sanitizer runtime):

```bash
cmake -B Builds-rtsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 -DWITH_REALTIME_SANITIZER=ON
cmake --build Builds-rtsan --target DspTests PluginTests ToolsTests MoodzFilterTools RtsanCanary Benchmarks
ctest --test-dir Builds-rtsan --output-on-failure    # rtsan-canary passes only if the sanitizer fires
./Builds-rtsan/MoodzFilterTools check
```

Audio-thread functions carry `MOODZ_NONBLOCKING` (`source/dsp/Realtime.h`): `[[clang::nonblocking]]` in that build, nothing elsewhere.

## Repo map

```
CLAUDE.md               ← you are here (router); AGENTS.md is a symlink to it
GLOSSARY.md             shared vocabulary
FUTURE.md               requested, deliberately not built
CMakeLists.txt          identity, formats, targets
VERSION                 single version number
source/
  dsp/                  signal processing, static library `dsp`, never includes JUCE (CMake enforces it)
  plugin/               JUCE AudioProcessor, parameters, state (SharedCode)
  ui/                   editor (SharedCode); createEditor() lives here
tools/                  MoodzFilterTools command line (render, compare, check); tools/core/ has no JUCE
tests/
  dsp/                  DspTests, no JUCE
  plugin/               PluginTests, with JUCE
  tools/                ToolsTests: test signals and error metrics, no JUCE
  golden/               reference renders + manifest.json (plugin ADR 0003)
benchmarks/             Catch2 benchmarks
cmake/                  Pamplejuce CMake modules (submodule)
JUCE/                   JUCE 9.0.5 (submodule, pinned tag)
modules/                melatonin_inspector (Debug only)
packaging/              Windows installer (Inno Setup)
docs/tasks/             atomic task files
.claude/
  STATE.md              current state, maintained by hand
  rules/                rules by domain (00-INDEX.md first)
  agents/               task-verifier, rt-safety-reviewer, index-writer
  choices/, learning/   templates (the plugin's own ADRs and post-mortems are in the playbook)
```

Built from the [Pamplejuce](https://github.com/sudara/pamplejuce) template (MIT, notice in `THIRD_PARTY_NOTICES.txt`). Its documentation: https://melatonin.dev/manuals/pamplejuce/.
