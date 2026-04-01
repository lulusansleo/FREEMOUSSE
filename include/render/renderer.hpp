#pragma once
#include "core/audio_state.hpp"
#include "render/bubble_system.hpp"
#include "render/logo.hpp"

#include <atomic>

struct SDL_Window;

namespace dv {

static constexpr int kWindowW = 1280;
static constexpr int kWindowH = 720;

class Renderer {
  public:
    explicit Renderer(SharedAudioState& state);
    ~Renderer();
    // Blocking event + draw loop. Returns when window is closed.
    void run(std::atomic<bool>& appRunning);

  private:
    void initSDL();
    void initOpenGL();
    void drawFrame(const AudioState& s);
    void shutdown();

    SharedAudioState& m_state;
    SDL_Window* m_window{nullptr};
    void* m_glContext{nullptr};
    BubbleSystem m_bubbles;
    Logo m_logo;
    bool m_logoLoaded{false};
    float m_timeSec{0.0f};
    unsigned int m_lastTicksMs{0};
    float m_logoBeatPulse{0.0f};
};

} // namespace dv
