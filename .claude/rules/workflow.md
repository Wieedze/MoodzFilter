# End of task workflow

## Minimum verification commands

```
cmake --build build
ctest --test-dir build --output-on-failure
build/<Plugin>Tools check
build/<Plugin>Tools compare build/renders tests/golden
pluginval --strictness-level 10 --validate <path to the .vst3>   # if plugin/ touched
```

The output of each command is pasted into the end of task report. "It compiles" without output does not count.

## Numeric claims

Any audio figure written in a report, a commit or a doc (dB, ms, samples of latency, % CPU) comes with the command that reproduces it. A figure without a command is removed.

## End of task order

1. Checks above, green.
2. `rt-safety-reviewer` if `dsp/` or `plugin/` touched.
3. `code-review`.
4. `task-verifier`.
5. If pass: post mortem in `.claude/learning/`, `STATE.md` updated, closing note in the task file.
6. Run the `index-writer` agent (`.claude/agents/index-writer.md`). It updates the files the index website is built from (roadmap, milestones, state) and pushes; the site redeploys from them. The repo stays the single source.

## Commits

* One commit = one coherent change. Message in the imperative.
* Audio reference changes are in their own `golden: …` commit.
* No commit that mixes refactor and feature.

## End of session

Before ending a work session, run `index-writer` once so the roadmap and `STATE.md` reflect where the work stopped.
