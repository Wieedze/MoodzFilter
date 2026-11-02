// MoodzFilterTools: headless command line for render, check, compare and bench.
// The commands arrive in later tasks (render, check and compare in task 0004).
#include <cstdio>
#include <string_view>

namespace
{
    constexpr int exitOk = 0;
    constexpr int exitUsage = 2;

    void printUsage()
    {
        std::puts ("Usage: MoodzFilterTools <command>\n"
                   "\n"
                   "Commands:\n"
                   "  --help    show this message\n"
                   "\n"
                   "render, check, compare and bench are not implemented yet.");
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        printUsage();
        return exitUsage;
    }

    const std::string_view command { argv[1] };

    if (command == "--help" || command == "-h" || command == "help")
    {
        printUsage();
        return exitOk;
    }

    std::fprintf (stderr, "Unknown command: %s\n\n", argv[1]);
    printUsage();
    return exitUsage;
}
