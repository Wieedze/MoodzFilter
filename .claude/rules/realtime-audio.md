# Audio thread rules

Reference: Ross Bencina, *Real-time audio programming 101: time waits for nothing*. Principle: **if you don't know how long it takes, don't do it on the audio thread.**

The "audio thread" covers `processBlock`, everything it calls, and `prepareToPlay`/`releaseResources` only for what is explicitly allowed there.

## Forbidden in `processBlock` and everything it calls

| Forbidden | Examples | Alternative |
| --- | --- | --- |
| Memory allocation / deallocation | `new`, `delete`, `malloc`, `std::vector::push_back` beyond capacity, `std::string`, capturing `std::function`, `juce::String`, `AudioBuffer::setSize` that grows | Preallocate in `prepareToPlay`; known max sizes |
| Blocking lock | `std::mutex::lock`, `CriticalSection`, `ScopedLock` | Atomics, lock-free SPSC FIFO, `try_lock` that skips the work if busy |
| I/O | Files, network, `DBG`, `std::cout`, logging | FIFO to a logging thread |
| System calls that can block | `sleep`, thread creation, `MessageManager` | Background thread, `AsyncUpdater` on the message thread side |
| Exceptions | `throw`, functions that can throw | Return codes, preconditions checked upstream |
| Waiting on the UI thread | Reading a shared `Component`, `Value`, `ValueTree` | Snapshot of atomic parameters |
| Algorithms with unbounded time | Sorting, searching a structure that grows | Fixed size, bounded and known complexity |

## Mandatory

* Any data shared between the UI and audio goes through an atomic, an SPSC FIFO, or an atomic pointer swap.
* A replaced heavy object (IR, table, sample) is **freed off the audio thread** (a "garbage" queue emptied by a timer).
* Denormals are neutralized (`juce::ScopedNoDenormals` at the top of `processBlock`).
* `prepareToPlay` may allocate; it must allocate for the worst case (`maxBlockSize`, max sample rate used).
* The host can call `processBlock` with fewer samples than `maxBlockSize`, including 0. The code handles it.

## How to check it

* Diff review by the `rt-safety-reviewer` agent.
* RealtimeSanitizer: `processBlock` and the DSP `process` functions marked `[[clang::nonblocking]]`, tests run with `-fsanitize=realtime` in CI.
* Ready-made primitives rather than homemade: `farbot::RealtimeObject`, `farbot::fifo`, `AbstractFifo` (see `docs/07_REUSE.md`).
* `tools bench`: the max cost of a block stays under budget.
