# FUTURE

Requested or imagined, deliberately not built now. One line of context each. The SPEC's "Out of v1" list (playbook `docs/plugins/moodzfilter/SPEC.md`) is not repeated here.

| Idea | Context | Date |
| --- | --- | --- |
| macOS build and AU format | Removed in task 0001 (v1 is Windows VST3 only). Bring back with signing and notarisation | 2026-11-02 |
| Sample-accurate parameter automation | Task 0003: parameters are read once per block and each change restarts a 20 ms ramp, so under continuous automation the gain curve depends on the host block size. Linked to the playbook open question on TetraDrive's sample-accurate automation patch | 2026-11-02 |
| Annotate `Snapshotter::snapshot` and `SmoothedGain::setTargetDb` with `MOODZ_NONBLOCKING` | Task 0005 rt-safety review: already covered at run time through `processBlock`; the annotation would document intent for direct callers | 2026-11-02 |
| Bound the template clear loop in `processBlock` by `buffer.getNumChannels()` | Task 0005 rt-safety review: in Release nothing stops an out-of-bounds write if a host passes fewer channels than `getTotalNumOutputChannels()`; JUCE wrappers normally do pass enough | 2026-11-02 |
| Guard against non-finite input samples | Task 0006 rt-safety review: NaN input stays NaN, and ±Inf input times the silence gain 0 gives NaN; output is non-finite only when the host sends non-finite input. A sanitising stage would make output always finite | 2026-11-02 |
| ThreadSanitizer suppressions for sst-filters | Task 0007 rt-safety review: `CytomicSVFQuadForm.h` writes function-local statics on every coefficient update and `TuningProvider::twoToThe` initialises a shared table with a plain bool; harmless here (one filter instance, table warmed up in `prepare`), but a TSan job would report them | 2026-11-02 |
| Flush denormals inside the dsp layer | Task 0007 rt-safety review: `MultimodeFilter` relies on the caller (processBlock) for FTZ/DAZ; DSP-level benchmarks must set it themselves or their timings are inflated | 2026-11-02 |
