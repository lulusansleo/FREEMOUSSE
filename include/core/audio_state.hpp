#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace dv {

static constexpr int kFFTSize       = 2048;
static constexpr int kSpectrumBins  = kFFTSize / 2 + 1;
static constexpr int kSampleRate    = 48000;
static constexpr int kCaptureFrames = 512;

// Plain data written by the analysis thread, read by the render thread.
struct AudioState {
    std::array<float, kSpectrumBins> spectrum{};
    float rms{0.f};      // Root-mean-square level [0..1]
    float bpm{0.f};      // Current BPM estimate
    bool  onBeat{false}; // True for the frame when a beat fires
};

// Double-buffered atomic swap — render thread always gets a consistent
// snapshot without blocking the analysis thread.
struct SharedAudioState {
    void write(const AudioState& s) noexcept
    {
        int next = 1 - m_writeIdx.load(std::memory_order_relaxed);
        m_buf[next] = s;
        m_writeIdx.store(next, std::memory_order_release);
    }

    AudioState read() const noexcept
    {
        return m_buf[m_writeIdx.load(std::memory_order_acquire)];
    }
private:
    AudioState       m_buf[2];
    std::atomic<int> m_writeIdx{0};
};

} // namespace dv
