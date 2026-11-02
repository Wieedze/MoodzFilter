#include "Commands.h"

#include "Rendering.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <set>

namespace moodz::tools
{
    namespace
    {
        bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer)
        {
            file.deleteFile(); // FileOutputStream appends to an existing file
            std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
            if (stream == nullptr)
                return false;

            juce::WavAudioFormat wav;
            const auto options = juce::AudioFormatWriterOptions {}
                                     .withSampleRate (renderSampleRate)
                                     .withNumChannels (buffer.getNumChannels())
                                     .withBitsPerSample (32)
                                     .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
            auto writer = wav.createWriterFor (stream, options);
            return writer != nullptr && writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        }

        struct LoadedWav
        {
            juce::AudioBuffer<float> buffer;
            double sampleRate = 0.0;
        };

        std::optional<LoadedWav> readWav (const juce::File& file)
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr)
                return std::nullopt;

            LoadedWav loaded;
            loaded.sampleRate = reader->sampleRate;
            loaded.buffer.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
            if (!reader->read (&loaded.buffer, 0, loaded.buffer.getNumSamples(), 0, true, true))
                return std::nullopt;
            return loaded;
        }

        struct Tolerance
        {
            double maxAbsError = 0.0;
            double maxRmsErrorDbfs = 0.0;
        };

        // Format documented in plugin ADR 0003 (playbook docs/plugins/moodzfilter/choices/).
        struct Manifest
        {
            std::map<juce::String, Tolerance> files;
        };

        std::optional<Manifest> readManifest (const juce::File& file, juce::String& error)
        {
            const auto json = juce::JSON::parse (file);
            if (!json.isObject())
            {
                error = "cannot read " + file.getFullPathName();
                return std::nullopt;
            }
            if (static_cast<int> (json.getProperty ("format", 0)) != 1)
            {
                error = "unsupported manifest format (expected 1)";
                return std::nullopt;
            }

            const auto defaults = json.getProperty ("defaults", {});
            const Tolerance base { defaults.getProperty ("maxAbsError", 0.0), defaults.getProperty ("maxRmsErrorDbfs", 0.0) };

            Manifest manifest;
            if (auto* files = json.getProperty ("files", {}).getDynamicObject())
            {
                for (const auto& entry : files->getProperties())
                {
                    Tolerance tolerance = base;
                    tolerance.maxAbsError = entry.value.getProperty ("maxAbsError", tolerance.maxAbsError);
                    tolerance.maxRmsErrorDbfs = entry.value.getProperty ("maxRmsErrorDbfs", tolerance.maxRmsErrorDbfs);
                    manifest.files[entry.name.toString()] = tolerance;
                }
            }
            if (manifest.files.empty())
            {
                error = "the manifest lists no file";
                return std::nullopt;
            }
            return manifest;
        }

        std::set<juce::String> wavFilesIn (const juce::File& directory)
        {
            std::set<juce::String> names;
            for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.wav"))
                names.insert (file.getFileName());
            return names;
        }

        bool isFinite (const juce::AudioBuffer<float>& buffer)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    if (!std::isfinite (buffer.getSample (channel, i)))
                        return false;
            return true;
        }

        Difference measureBuffers (const juce::AudioBuffer<float>& actual, const juce::AudioBuffer<float>& expected)
        {
            Difference worst { 0.0, silenceDbfs };
            for (int channel = 0; channel < actual.getNumChannels(); ++channel)
            {
                const auto size = static_cast<size_t> (actual.getNumSamples());
                const auto difference = measureDifference ({ actual.getReadPointer (channel), size }, { expected.getReadPointer (channel), size });
                worst.maxAbsError = std::max (worst.maxAbsError, difference.maxAbsError);
                worst.rmsErrorDbfs = std::max (worst.rmsErrorDbfs, difference.rmsErrorDbfs);
            }
            return worst;
        }
    }

    int runRender (const juce::File& outputDirectory)
    {
        if (!outputDirectory.createDirectory())
        {
            std::cerr << "cannot create " << outputDirectory.getFullPathName() << "\n";
            return exitUsage;
        }

        const auto signals = testSignals();
        for (const auto& renderCase : renderCases())
        {
            for (const auto& signal : signals)
            {
                const auto output = renderThroughPlugin (renderCase, signal.samples, referenceBlockSize);
                const auto file = outputDirectory.getChildFile (renderFileName (renderCase, signal));
                if (!writeWav (file, output))
                {
                    std::cerr << "cannot write " << file.getFullPathName() << "\n";
                    return exitUsage;
                }
                std::cout << "wrote " << file.getFileName() << "\n";
            }
        }
        return exitOk;
    }

    int runCompare (const juce::File& renderDirectory, const juce::File& goldenDirectory)
    {
        juce::String error;
        const auto manifest = readManifest (goldenDirectory.getChildFile ("manifest.json"), error);
        if (!manifest)
        {
            std::cerr << "manifest: " << error << "\n";
            return exitUsage;
        }

        bool allPassed = true;
        auto fail = [&] (const juce::String& name, const juce::String& reason) {
            std::cout << "FAIL " << name << ": " << reason << "\n";
            allPassed = false;
        };

        // Every render and every reference must be listed, so nothing is silently skipped
        for (const auto& name : wavFilesIn (renderDirectory))
            if (!manifest->files.contains (name))
                fail (name, "render not listed in manifest.json");
        for (const auto& name : wavFilesIn (goldenDirectory))
            if (!manifest->files.contains (name))
                fail (name, "reference not listed in manifest.json");

        for (const auto& [name, tolerance] : manifest->files)
        {
            const auto render = readWav (renderDirectory.getChildFile (name));
            const auto golden = readWav (goldenDirectory.getChildFile (name));
            if (!render)
            {
                fail (name, "render missing or unreadable");
                continue;
            }
            if (!golden)
            {
                fail (name, "reference missing or unreadable");
                continue;
            }
            if (render->buffer.getNumChannels() != golden->buffer.getNumChannels()
                || render->buffer.getNumSamples() != golden->buffer.getNumSamples()
                || std::abs (render->sampleRate - golden->sampleRate) > 0.5)
            {
                fail (name, "channel count, length or sample rate differs");
                continue;
            }

            const auto difference = measureBuffers (render->buffer, golden->buffer);
            const bool passed = difference.maxAbsError <= tolerance.maxAbsError && difference.rmsErrorDbfs <= tolerance.maxRmsErrorDbfs;
            const auto line = juce::String ("max error ") + juce::String (difference.maxAbsError, 9)
                              + " (limit " + juce::String (tolerance.maxAbsError, 9) + "), RMS error "
                              + juce::String (difference.rmsErrorDbfs, 1) + " dBFS (limit "
                              + juce::String (tolerance.maxRmsErrorDbfs, 1) + ")";
            if (passed)
                std::cout << "ok   " << name << ": " << line << "\n";
            else
                fail (name, line);
        }

        std::cout << (allPassed ? "compare: all files within tolerance\n" : "compare: FAILED\n");
        return allPassed ? exitOk : exitFailed;
    }

    int runCheck()
    {
        // Block size independence through the whole plugin, against the reference block size
        constexpr double tolerance = 1e-7;
        bool allPassed = true;
        const auto signals = testSignals();

        for (const auto& renderCase : renderCases())
        {
            for (const auto& signal : signals)
            {
                const auto name = renderFileName (renderCase, signal);
                const auto reference = renderThroughPlugin (renderCase, signal.samples, referenceBlockSize);
                if (!isFinite (reference))
                {
                    std::cout << "FAIL " << name << ": non-finite output\n";
                    allPassed = false;
                    continue;
                }

                bool filePassed = true;
                for (int blockSize : { 1, 64, 480 })
                {
                    const auto output = renderThroughPlugin (renderCase, signal.samples, blockSize);
                    const auto difference = measureBuffers (output, reference);
                    if (!isFinite (output) || difference.maxAbsError > tolerance)
                    {
                        std::cout << "FAIL " << name << ": block size " << blockSize << " differs by "
                                  << difference.maxAbsError << " from block size " << referenceBlockSize << "\n";
                        filePassed = false;
                    }
                }
                if (filePassed)
                    std::cout << "ok   " << name << "\n";
                else
                    allPassed = false;
            }
        }

        std::cout << (allPassed ? "check: all passed\n" : "check: FAILED\n");
        return allPassed ? exitOk : exitFailed;
    }
}
