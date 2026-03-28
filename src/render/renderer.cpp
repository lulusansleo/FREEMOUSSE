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
#include <vector>

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
    // If using GLAD, call gladLoadGLLoader here before any gl* calls.
    m_shaderProgram = loadShaderProgram("shaders/2d/spectrum.vert", "shaders/2d/spectrum.frag");
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // Reserve: kSpectrumBins bars * 6 vertices (2 triangles) * 2 floats (x,y)
    glBufferData(GL_ARRAY_BUFFER, kSpectrumBins * 6 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
}

void Renderer::drawFrame(const AudioState& s) {
    const float flash = s.onBeat ? 0.08f : 0.0f;
    glClearColor(0.03f + flash, 0.035f + flash * 0.5f, 0.05f + flash * 0.2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Build bar quads in normalized [0..1] coordinates.
    constexpr int kBars = 128;
    constexpr float kMinX = 0.02f;
    constexpr float kMaxX = 0.98f;
    constexpr float kBaseY = 0.03f;
    constexpr float kTopY = 0.96f;
    constexpr float kGap = 0.15f; // fraction of each slot reserved as spacing

    std::vector<float> verts;
    verts.reserve(kBars * 6 * 2);

    const float usableW = kMaxX - kMinX;
    const float slotW = usableW / static_cast<float>(kBars);
    const float barW = slotW * (1.0f - kGap);

    const int bins = kSpectrumBins;
    for (int i = 0; i < kBars; ++i) {
        const int b0 = (i * bins) / kBars;
        const int b1 = ((i + 1) * bins) / kBars;
        const int span = std::max(1, b1 - b0);

        float e = 0.0f;
        for (int b = b0; b < b1; ++b)
            e += s.spectrum[b];
        e /= static_cast<float>(span);

        // Compress dynamic range and emphasize visible motion.
        const float mag = std::clamp(std::log1p(e * 120.0f) / std::log1p(120.0f), 0.0f, 1.0f);
        const float beatBoost = s.onBeat ? 1.12f : 1.0f;
        const float h = std::clamp(std::pow(mag, 0.72f) * beatBoost, 0.0f, 1.0f);

        const float x0 = kMinX + i * slotW + (slotW - barW) * 0.5f;
        const float x1 = x0 + barW;
        const float y0 = kBaseY;
        const float y1 = y0 + h * (kTopY - y0);

        // Triangle 1
        verts.push_back(x0);
        verts.push_back(y0);
        verts.push_back(x1);
        verts.push_back(y0);
        verts.push_back(x1);
        verts.push_back(y1);
        // Triangle 2
        verts.push_back(x0);
        verts.push_back(y0);
        verts.push_back(x1);
        verts.push_back(y1);
        verts.push_back(x0);
        verts.push_back(y1);
    }

    glUseProgram(m_shaderProgram);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                    verts.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 2));
}

void Renderer::shutdown() {
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
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
