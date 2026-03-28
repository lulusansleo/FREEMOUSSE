#pragma once
#include <atomic>
#include <memory>

namespace dv {

class AudioCapture;
class Analyzer;
class Renderer;

// Top-level owner — creates subsystems, wires threads, drives the run loop.
class App {
public:
    App();
    ~App();
    App(const App&)            = delete;
    App& operator=(const App&) = delete;

    int run(); // blocking; returns when window is closed
private:
    void shutdownSubsystems();

    std::unique_ptr<AudioCapture> m_capture;
    std::unique_ptr<Analyzer>     m_analyzer;
    std::unique_ptr<Renderer>     m_renderer;
    std::atomic<bool>             m_running{true};
};

} // namespace dv
