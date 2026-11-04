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
| **Canary** | A test program that deliberately breaks a rule (for example `RtsanCanary`, which allocates in a nonblocking function) to prove that a checker such as RealtimeSanitizer really fires; its test passes only if the checker reports the violation |
| **Golden manifest** | `tests/golden/manifest.json`: lists every golden reference with its tolerances (max absolute error, max RMS error in dBFS); a render or reference not listed fails `compare`. Format: plugin ADR 0003 |
| **Generic editor** | `juce::GenericAudioProcessorEditor`: the editor JUCE draws by itself, one control per parameter. The v1 interface (SPEC) |
| **Filter model** | One filter design from sst-filters (`sst::filtersplusplus::FilterModel`: Cytomic SVF, OB-Xd 4-pole, Diode Ladder…) with its configuration (passband, slope, drive, submodel): a `FilterSetup` in `dsp/` |
| **Passband** | What a filter lets through: low pass, high pass, band pass, notch (`sst::filtersplusplus::Passband`). The SPEC called it the filter mode; since ADR 0007 it is part of each Model entry |
| **Filter candidate** | One of the 7 filter models auditioned in task 0008 to choose the v1 models. Historical: the candidate list left the code in task 0009 |
| **Filter list** | The v1 entries of the Model parameter in their frozen order: each of the 5 models (Cytomic SVF, Vintage Ladder, OB-Xd 4-pole, Diode Ladder, K35) with each passband it has, 9 entries (playbook ADRs 0006, 0007): `dsp::filterSetups()` |
| **Filter setup** | A `FilterSetup`: one filter model in one configuration. Each entry of the filter list is one, holding its default configuration |
| **Filter variant** | A filter setup the parameters can reach: the Model entries, plus one per Vintage variant and per Diode Ladder slope step, 17 in all (ADR 0008): `dsp::filterVariants()`. Each has its own prepared filter in the processor; `dsp::chooseFilter` picks one per block |
| **Crossfade** | When the filter variant changes, the outgoing and incoming variants run side by side for 20 ms and their outputs are mixed linearly, so the change does not click (task 0011) |
| **Oversampling factor** | How many times the sample rate the filter runs at: 1 (Off), 2 or 4; `dsp::Oversampler` goes up and down with a linear phase FIR |
| **Oversampled filter chain** | `dsp::OversampledFilterChain`: one crossfading filter bank per oversampling factor, each with its oversampler and its delay to the constant latency, and the crossfade between two of them |
| **Constant latency** | The one latency the plugin reports at every Oversampling setting: Off and 2x are delayed to the 4x round trip (ADR 0009) |
| **Crossfading filter bank** | `dsp::CrossfadingFilterBank`: every filter variant prepared once, the selected one playing, and the crossfade between two of them; a choice made during a crossfade waits for it to end |
| **Click measure** | Largest absolute second difference of the output, `|y[n] − 2 y[n−1] + y[n−2]|`, after a change; a 200 Hz sine at −12 dBFS gives about 1.7e-4 on its own (task 0011 tests) |
| **Per-model setting** | A parameter that only some models use, ignored by the others: Slope, K35 saturation, Vintage variant (ADR 0008) |
| **Slope** | How fast a low-pass filter attenuates above the cutoff, in dB per octave, 6 to 24. The OB-Xd 4-pole plays it continuously (Morph), the Diode Ladder at the nearest 6 dB step |
| **Morph** | The OB-Xd 4-pole mode that crossfades the outputs of its four poles, so the slope moves continuously from 6 to 24 dB (sst-filters `Slope_Morph`) |
| **K35 saturation** | Drive of the tanh stage inside the K35 filter, 0 to 100 % (sst-filters 0 to 4; 25 % = Mild) |
| **Vintage variant** | The Vintage Ladder sub-model: Runge-Kutta, Huovilainen or Huovilainen 2010, each with or without gain compensation |
| **Extra value** | The model's continuous setting passed to sst-filters with the cutoff and resonance (`makeCoefficients`): OB-Xd Morph, K35 saturation; 0..1 |
| **Silence floor** | Lowest value of a gain control, −60 dB internally: displayed `-inf`, gives a gain of exactly 0 (ADR 0004) |
| **Render case** | A named parameter setup (and optional automation) that `MoodzFilterTools render` applies to every test signal; one render case × one test signal = one golden reference |
| **Test signal** | Standardised input of a test: impulse, sine, sweep, pink noise, loop |
| **Gate** | Condition to meet to move from one phase to the next |
| **Layer** | One of `dsp` (signal processing, no JUCE), `plugin` (JUCE processor, parameters, state), `ui` (editor). Each lives in its own folder of `source/`; only `plugin` and `ui` see JUCE |
| **Tools** | The `MoodzFilterTools` command line: render, check, compare, bench. Development only, never shipped |
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
