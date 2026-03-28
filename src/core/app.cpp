#include "core/app.hpp"
#include "audio/audio_capture.hpp"
#include "audio/ring_buffer.hpp"
#include "analysis/analyzer.hpp"
#include "render/renderer.hpp"

namespace dv {

// Module-level singletons owned by App (one per process)
static RingBuffer       s_ring{8};
static SharedAudioState s_state;

App::App()
{
    m_capture = std::make_unique<AudioCapture>([](const float* buf, std::size_t n)
    {
        // TODO: mix stereo → mono, push SampleChunk into s_ring.
        // RULES: no malloc, no locks, no blocking calls here.
        (void)buf; (void)n;
    });

    m_analyzer = std::make_unique<Analyzer>(s_ring, s_state);
    m_renderer = std::make_unique<Renderer>(s_state);
}

App::~App() { shutdownSubsystems(); }

int App::run()
{
    m_capture->start();
    m_analyzer->start();
    m_renderer->run(m_running); // blocks until window closed / ESC
    shutdownSubsystems();
    return 0;
}

void App::shutdownSubsystems()
{
    m_running.store(false);
    if (m_capture)  m_capture->stop();
    if (m_analyzer) m_analyzer->stop();
}

} // namespace dv
