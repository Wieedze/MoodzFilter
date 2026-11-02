#include <catch2/catch_test_macros.hpp>

// The dsp layer must stay JUCE-free (.claude/rules/dsp.md). DspTests is built with only
// the dsp library and Catch2 on its include path, so JUCE headers must be unreachable.
TEST_CASE ("dsp layer", "[dsp][layer]")
{
    SECTION ("JUCE is not reachable")
    {
#if __has_include(<juce_core/juce_core.h>)
        constexpr bool juceReachable = true;
#else
        constexpr bool juceReachable = false;
#endif
        CHECK_FALSE (juceReachable);
    }
}
