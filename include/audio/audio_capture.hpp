#pragma once
#include <atomic>
#include <cstddef>
#include <functional>
#include <portaudio.h>

namespace dv {

static constexpr int kChannels = 2; // stereo; sum to mono before analysis

// Delivered on the RT audio thread — NO allocation, NO locks, NO I/O.
using AudioCallback = std::function<void(const float* buf, std::size_t frames)>;

class AudioCapture {
  public:
    explicit AudioCapture(AudioCallback cb);
    ~AudioCapture();
    void start();
    void stop();
    bool isRunning() const noexcept { return m_running.load(); }

  private:
    static int paCallback(const void* input, void* output, unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags flags,
                          void* userData);

    struct Impl;
    Impl* m_impl{nullptr};
    AudioCallback m_cb;
    std::atomic<bool> m_running{false};
};

} // namespace dv
