# C++ rules

* C++23 (C++20 minimum). No compiler extensions.
* The repo's `clang-format`, applied before commit.
* Warnings: `-Wall -Wextra` (or `/W4`); no new warning tolerated.
* RAII everywhere. No bare `new`/`delete`; `std::unique_ptr` off the audio thread.
* `const` and `noexcept` by default on what runs on the audio thread.
* Pass buffers as `std::span<float>` (or `juce::dsp::AudioBlock` in `plugin/`), never as separate pointer + size in a new API.
* No `float` compared with `==` in tests: explicit tolerance.
* One class = one responsibility. Deep modules (a lot of behavior behind a small interface) are preferred over scattered utility classes.
* Names taken from `GLOSSARY.md`. A term missing from the glossary is added to it in the same task.
* No macros except for build configuration.
* Comments: the why, and the source of a DSP formula (paper, page, equation). Not the what.
