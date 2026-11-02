# JUCE rules

## Parameters

* All parameters are declared in a single file (`plugin/Parameters.*`): ID, name, range, default, displayed text.
* IDs are constants, never repeated strings.
* **After the first release, an ID is never renamed or deleted.** An obsolete parameter stays declared and ignored. A test freezes the list.
* Parameter version (`ParameterID{ id, version }`) incremented on every addition.
* The DSP never reads the APVTS: the `plugin/` layer fills a `Params` snapshot per block.
* Invariants between parameters (order, minimum gaps) applied in the snapshot and tested.

## State

* The saved state carries a version number.
* `setStateInformation` can load every past version; each missing value has a documented fallback.
* List what is **not** in presets (bypass, solo, session settings).
* State files from each published version are kept in `tests/plugin/states/`.

## Processor

* `setLatencySamples` called as soon as the latency changes, and latency kept constant as much as possible.
* `getTailLengthSeconds` filled in for effects with a tail (reverb, delay).
* Bus layouts: declare precisely what is supported in `isBusesLayoutSupported`.
* No access to the editor from the processor.

## Updating JUCE

* Never in a feature task. Dedicated task, ADR, reapplication of the patches in `patches/`, full CI.
