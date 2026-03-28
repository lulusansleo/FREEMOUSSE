#pragma once
#include "audio/ring_buffer.hpp"
#include "core/audio_state.hpp"

#include <atomic>
#include <memory>
#include <thread>

namespace dv {

// Runs on a dedicated thread (not the RT audio thread).
// Reads SampleChunks from the ring buffer, runs FFTW + aubio,
// and writes results into SharedAudioState.
class Analyzer {
  public:
    Analyzer(RingBuffer& ring, SharedAudioState& state);
    ~Analyzer();
    void start();
    void stop();

  private:
    void loop(); // thread entry point
    RingBuffer& m_ring;
    SharedAudioState& m_state;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace dv
