#pragma once

// Marks a function that must never allocate, lock or block (audio thread). In the
// RealtimeSanitizer build (-DWITH_REALTIME_SANITIZER=ON, clang >= 20) it becomes
// [[clang::nonblocking]], which RTSan checks at run time; elsewhere it expands to nothing,
// since other compilers do not know the attribute. Build configuration only
// (.claude/rules/cpp.md), see .claude/rules/realtime-audio.md "How to check it".
#if defined(MOODZ_REALTIME_SANITIZER) && defined(__clang__)
    #define MOODZ_NONBLOCKING [[clang::nonblocking]]
#else
    #define MOODZ_NONBLOCKING
#endif
