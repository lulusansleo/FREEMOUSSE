#pragma once
#include "core/audio_state.hpp"
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
    SDL_Window*  m_window{nullptr};
    void*        m_glContext{nullptr};
    unsigned int m_shaderProgram{0};
    unsigned int m_vao{0}, m_vbo{0};
};

} // namespace dv
