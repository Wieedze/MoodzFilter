# Glossary

Vocabulary shared between you, the agents and the code (Matt Pocock convention: class, file and test names follow these terms). A plugin project copies this file and extends it with its own terms.

| Term | Definition |
| --- | --- |
| **Audio thread** | The host thread that calls `processBlock`. Real-time: it must never wait |
| **Message thread** | The JUCE UI thread. It can allocate, block, do I/O |
| **Block** | One call to `processBlock`: N samples per channel, N variable and ≤ `maxBlockSize` |
| **Internal packet** | Fixed subdivision of a block used by the DSP to be independent of the block size |
| **Parameter snapshot** (`Params`) | Plain copy of the parameter values, made at the start of each block, the only thing the DSP reads |
| **APVTS** | `AudioProcessorValueTreeState`, the JUCE parameter tree, on the `plugin/` side only |
| **Parameter ID** | Stable identifier of a parameter, frozen after release |
| **State** | What the host saves in the session (`getStateInformation`), versioned |
| **Preset** | Subset of the state chosen by the user; we list what is not part of it |
| **State migration** | Loading a state from an earlier version with fallback values |
| **Latency** | Delay in samples introduced by the plugin, reported to the host |
| **Tail** | Time during which the plugin produces sound after the input ends |
| **Smoothing** | Interpolating a parameter over time to avoid zipper noise |
| **Oversampling** | Processing at a multiple of the sample rate to limit aliasing from non-linearities |
| **Golden reference** | Reference WAV render in `tests/golden/`, compared on every PR |
| **Test signal** | Standardised input of a test: impulse, sine, sweep, pink noise, loop |
| **Gate** | Condition to meet to move from one phase to the next |
| **Vertical slice** | Small piece of functionality that crosses all the layers it needs and can be tested on its own |
| **ADAA** | Antiderivative anti-aliasing: reduces the aliasing of a saturation without oversampling everything |
| **ZDF** | Zero delay feedback: virtual analogue filter with no delay in the feedback loop |
| **BLEP** | Correction of an oscillator's discontinuities to remove aliasing |
| **WDF** | Wave digital filter: modelling a circuit component by component |
| **RTSan** | clang's RealtimeSanitizer: makes a test fail if a function marked nonblocking allocates or locks |
| **pluginval** | Tool that loads the VST3 like a host and runs a battery of tests on it |
| **ADR** | Architecture decision record: a non-obvious decision, dated, with its reasons |
| **Post-mortem** | Review of a finished task: what was delivered, what got stuck, what we change |
| **Milestone** | Notable step of the project (scope frozen, gate passed, release), recorded in `docs/MILESTONES.md` |
