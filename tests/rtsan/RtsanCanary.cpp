// Proves that the RealtimeSanitizer build really catches a real-time violation: this
// program allocates inside a function marked nonblocking and must be stopped by RTSan.
// ctest declares it WILL_FAIL, so the test passes only when the sanitizer fires.
#include "dsp/Realtime.h"

#include <cstdlib>

namespace
{
    volatile void* sink = nullptr;

    void allocateOnTheAudioThread() MOODZ_NONBLOCKING
    {
        sink = std::malloc (64);
    }
}

int main()
{
    allocateOnTheAudioThread();
    std::free (const_cast<void*> (sink));
    return 0;
}
