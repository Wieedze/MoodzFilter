# Scope rules (anti drift)

A plugin drifts fast: a modulation matrix that goes from 4 to 17 sources, LFO shapes "because it's easy", a UI redone before the sound is right. These rules exist for that.

## The documents that hold the scope

| Document | Role |
| --- | --- |
| `docs/SPEC.md` | One page. What v1 does and an **Out of v1** section |
| `docs/tasks/NNNN-*.md` | One atomic task, with **Out of scope** |
| `FUTURE.md` | Everything requested or imagined and not built now |
| `.claude/STATE.md` | Current phase and gate to pass |

## Rules

1. **Nothing gets coded if it is not in the current task.** An idea that comes up along the way goes into `FUTURE.md` with one line of context, then return to the task.
2. **A task fits in one session.** Otherwise it is split again before starting.
3. **Phases are closed by gates** (`docs/06_START_PLAN.md`). Do not start the UI before the DSP gate is green.
4. **Changing the SPEC is a decision**, not a consequence: `/grill-with-docs`, then ADR, then amended SPEC.
5. **When an agent hesitates about scope, it stops and asks.** It does not pick the broadest option.
6. **No "small refactor along the way"** in a feature task. Refactor = dedicated task.
7. **Sound comes before UI**: no visual work on a feature whose DSP has not passed its tests and the listening check.
8. **Do not code what exists.** Before writing a component, apply the selection rule of `docs/07_REUSE.md`: JUCE, then MIT/BSD, then GPL if the plugin is AGPL, then only our own code. The search done is noted in the task.

## Drift signals (to be checked by the task-verifier)

* Files modified outside the layer announced by the task.
* Parameters added that are not listed in the task.
* Diff much bigger than the task suggested.
* New term used without an entry in `GLOSSARY.md`.
