# Rules index

Load only the files that match the task (routing table in `CLAUDE.md`).

| File | Domain | Load when |
| --- | --- | --- |
| `realtime-audio.md` | Audio thread | `dsp/` or `plugin/` touched |
| `cpp.md` | Modern C++, style | Any C++ code |
| `dsp.md` | Signal processing | `dsp/` touched |
| `juce.md` | JUCE, parameters, state | `plugin/` or `ui/` touched |
| `ui.md` | Editor, components | `ui/` touched |
| `testing.md` | Tests, golden, TDD | Any code task |
| `build.md` | CMake, CI, packaging | Build or CI touched |
| `scope.md` | Scope, anti drift | Any new feature |
| `workflow.md` | End of task, verification | Any task, at the end |

Changing a rule: edit the file, write an ADR in `.claude/choices/` that says why, and link the post mortem if there is one.
