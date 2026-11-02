// MoodzFilterTools: headless command line driving the real PluginProcessor.
// render, compare and check (task 0004); bench comes later.
#include "Commands.h"

#include <juce_events/juce_events.h>

#include <iostream>
#include <string_view>

namespace
{
    void printUsage()
    {
        std::cout << "Usage: MoodzFilterTools <command>\n"
                     "\n"
                     "Commands:\n"
                     "  render <dir>                 write one WAV per render case and test signal\n"
                     "  compare <renders> <golden>   compare the renders with the references of <golden>/manifest.json\n"
                     "  check                        finite output and block size independence through the plugin\n"
                     "  snapshot <file.png>          PNG of the plugin editor, for review\n"
                     "  --help                       show this message\n"
                     "\n"
                     "Exit codes: 0 success, 1 check failed, 2 usage or input error.\n";
    }

    juce::File argumentAsFile (const char* argument)
    {
        return juce::File::getCurrentWorkingDirectory().getChildFile (argument);
    }
}

int main (int argc, char* argv[])
{
    using namespace moodz::tools;

    if (argc < 2)
    {
        printUsage();
        return exitUsage;
    }

    // The processor and its parameters expect JUCE to be initialised, as in a host
    juce::ScopedJuceInitialiser_GUI juce;

    const std::string_view command { argv[1] };

    if (command == "--help" || command == "-h" || command == "help")
    {
        printUsage();
        return exitOk;
    }
    if (command == "render" && argc == 3)
        return runRender (argumentAsFile (argv[2]));
    if (command == "compare" && argc == 4)
        return runCompare (argumentAsFile (argv[2]), argumentAsFile (argv[3]));
    if (command == "check" && argc == 2)
        return runCheck();
    if (command == "snapshot" && argc == 3)
        return runSnapshot (argumentAsFile (argv[2]));

    std::cerr << "Unknown command or wrong arguments: " << argv[1] << "\n\n";
    printUsage();
    return exitUsage;
}
