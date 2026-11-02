# Testing rules

* **TDD**: the test that describes the behavior is written and seen red before the code. One vertical slice at a time.
* A DSP test names its input signal, its parameters, the expected value, the tolerance, and where the expected value comes from.
* No test that reproduces the implementation to compute the expected value.
* Explicit tolerances, never exact equality on floats.
* DSP tests run without JUCE and in less than a few seconds.
* A fixed bug = a test that failed on that bug.
* **Audio references (`tests/golden/`)**: an agent never regenerates them to make a test pass. Change = separate commit `golden: <reason>`, validated by the human after listening.
* pluginval strictness 10 before any PR that touches `plugin/`.
* A disabled test carries a link to a task that re-enables it.
