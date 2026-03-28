#pragma once
#include "core/audio_state.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>

namespace dv {

class AudioCapture;
class Analyzer;
class Renderer;

// Top-level owner — creates subsystems, wires threads, drives the run loop.
class App {
  public:
    explicit App(std::string audioFilePath = {});
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run(); // blocking; returns when window is closed
  private:
    void pushMonoSample(float sample) noexcept;
    void pushInterleavedStereo(const float* buf, std::size_t frames) noexcept;
    void startFileSource();
    void stopFileSource();
    void fileSourceLoop();
    void shutdownSubsystems();

    std::string m_audioFilePath;
    std::unique_ptr<AudioCapture> m_capture;
    std::unique_ptr<Analyzer> m_analyzer;
    std::unique_ptr<Renderer> m_renderer;
    std::thread m_fileThread;
    std::array<float, kCaptureFrames> m_pendingChunk{};
    std::size_t m_pendingFill{0};
    std::atomic<bool> m_running{true};
    std::atomic<bool> m_fileSourceFailed{false};
};

} // namespace dv
