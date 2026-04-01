#include "render/renderer.hpp"

#include "render/shader.hpp"

#include <SDL.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dv {

Renderer::Renderer(SharedAudioState& state) : m_state(state) {}
Renderer::~Renderer() {
    shutdown();
}

void Renderer::run(std::atomic<bool>& appRunning) {
    initSDL();
    initOpenGL();

    SDL_Event e;
    while (appRunning.load()) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                appRunning.store(false);
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                appRunning.store(false);
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                glViewport(0, 0, e.window.data1, e.window.data2);
        }
        drawFrame(m_state.read());
        SDL_GL_SwapWindow(m_window);
    }
    shutdown();
}

void Renderer::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    m_window = SDL_CreateWindow("DJ Visualizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                kWindowW, kWindowH, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_window)
        throw std::runtime_error(SDL_GetError());

    m_glContext = SDL_GL_CreateContext(m_window);
    SDL_GL_SetSwapInterval(0); // vsync OFF — we drive our own frame pacing
}

void Renderer::initOpenGL() {
    glViewport(0, 0, kWindowW, kWindowH);
    m_bubbles.init();
    m_logo.load("assets/FREEMOUSSE LOGO.jpg");
    m_logoLoaded = true;
    m_lastTicksMs = SDL_GetTicks();
}

void Renderer::drawFrame(const AudioState& s) {
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    const unsigned int now = SDL_GetTicks();
    const float dt = std::min(0.05f, static_cast<float>(now - m_lastTicksMs) * 0.001f);
    m_lastTicksMs = now;
    m_timeSec += dt;

    m_bubbles.update(dt, s, m_timeSec);
    m_bubbles.draw(m_timeSec);

    if (m_logoLoaded) {
        if (s.onBeat)
            m_logoBeatPulse = 1.0f;
        m_logoBeatPulse = std::max(0.0f, m_logoBeatPulse - dt * 3.6f);

        const float pulse = 1.0f + 0.12f * m_logoBeatPulse;
        m_logo.draw(pulse, pulse, 0.97f);
    }
}

void Renderer::shutdown() {
    m_logoLoaded = false;
    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

} // namespace dv
