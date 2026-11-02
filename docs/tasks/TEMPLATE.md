# NNNN — Short title in the imperative

**Phase**: 0 / 1 / 2 / 3 / 4 / 5
**Layer**: dsp / plugin / ui / build / docs
**Depends on**: NNNN, NNNN

## Goal

One sentence: the observable behaviour at the end.

## Expected behaviour (measurable)

| Test signal | Parameters | Expected result | Tolerance |
| --- | --- | --- | --- |
| 1 kHz sine, −12 dBFS | gain = +6 dB | output at −6 dBFS | ± 0.01 dB |

## Out of scope

What this task does **not** do, even if it is tempting. Any addition goes into `FUTURE.md`.

* …

## Rules to load

* `.claude/rules/…`

## Skills

* `tdd`

## Definition of done

* [ ] Tests written before the code, red then green.
* [ ] `cmake --build build` with no new warning.
* [ ] `ctest --test-dir build` green.
* [ ] `tools check` green; `tools compare` green or reference changed by a justified `golden:` commit.
* [ ] pluginval 10 green if `plugin/` touched.
* [ ] `rt-safety-reviewer` passed if `dsp/` or `plugin/` touched.
* [ ] `GLOSSARY.md` up to date if a new term appears.
* [ ] ADR written if a non-obvious decision was made.
* [ ] `task-verifier`: pass. Post-mortem written, `STATE.md` updated.

## End note (filled by the task-verifier)
