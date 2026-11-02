# MoodzFilter

A multi-model filter effect with drive and a tempo-synced LFO, by Radikal Moodz. VST3, Windows. Work in progress.

Scope, roadmap and decisions: https://wieedze.github.io/VstKnowledgeBase-/

## Build

```bash
git clone --recurse-submodules https://github.com/Wieedze/MoodzFilter
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build Builds
ctest --test-dir Builds --output-on-failure
```

The VST3 lands in `Builds/MoodzFilter_artefacts/Release/VST3/`.

## Licence

AGPLv3 (`LICENSE`). Built with [JUCE](https://juce.com) 9 under its AGPLv3 option, from the [Pamplejuce](https://github.com/sudara/pamplejuce) template (MIT, see `THIRD_PARTY_NOTICES.txt`).
