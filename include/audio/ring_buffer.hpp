#pragma once
// Thin wrapper around rigtorp/SPSCQueue.
// Writer = RT audio thread.  Reader = analysis thread.
#include "core/audio_state.hpp"

#include <array>
#include <rigtorp/SPSCQueue.h>

namespace dv {

using SampleChunk = std::array<float, kCaptureFrames>; // mono
using RingBuffer = rigtorp::SPSCQueue<SampleChunk>;    // 8 slots

} // namespace dv
