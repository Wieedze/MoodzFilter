# DSP rules

* `dsp/` does not depend on JUCE. No `#include <juce_...>`.
* Each processor exposes `prepare(double sampleRate, int maxBlockSize)`, `reset()`, `process(...)`. No global state.
* Every formula cites its source in a comment (RBJ cookbook, paper, book, page).
* Coefficients are recomputed outside the per-sample loop, and only when a parameter changes.
* Every audible parameter is smoothed.
* Nonlinearities (saturation, waveshaping, clipping): oversampling around them only, latency computed and reported.
* Result independent of block size. If an algorithm requires it, process in fixed-size internal chunks (Drum Foundry: 32 samples in its reverb). The block-size independence test enables modulation.
* Finite output guaranteed: protect divisions, `log`s, filters unstable at the bounds.
* No `rand()`: deterministic, seedable generator, so test renders are reproducible.
* An algorithm inspired by a third-party project cites the project and its license; if the project is GPL/AGPL and code is reused, an ADR is mandatory.
