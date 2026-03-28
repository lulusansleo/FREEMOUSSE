#pragma once
// Thin wrapper around rigtorp/SPSCQueue.
// Writer = RT audio thread.  Reader = analysis thread.
#include <rigtorp/SPSCQueue.h>
#include <array>
#include "core/audio_state.hpp"

namespace dv {

using SampleChunk = std::array<float, kCaptureFrames>; // mono
using RingBuffer  = rigtorp::SPSCQueue<SampleChunk>;   // 8 slots

} // namespace dv
