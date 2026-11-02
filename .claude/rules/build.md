# Build and CI rules

* CMake ≥ 3.25, Ninja generator by default.
* JUCE as a submodule, pinned to a tag. JUCE patches live in `patches/` with a one-line explanation each.
* Third-party dependencies via CPM with a pinned version (no moving branch).
* Targets: `dsp` (library without JUCE), `<Plugin>` (formats), `Tests`, `Benchmarks`, `<Plugin>Tools` (option, off in release).
* Single version in `VERSION` (or the first heading of `CHANGELOG.md`), propagated to the binary. CI checks consistency.
* CI on every PR: Linux, Windows, macOS; build, `ctest`, `tools check`, `tools compare`, pluginval 10, benchmarks, clang-format.
* Release: a `vX.Y.Z` tag consistent with the version triggers the build of the installers.
* Binaries and installers are never committed; they are CI artifacts.
