#include "render/renderer.hpp"
#include "render/shader.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdexcept>

namespace dv {

Renderer::Renderer(SharedAudioState& state) : m_state(state) {}
Renderer::~Renderer() { shutdown(); }

void Renderer::run(std::atomic<bool>& appRunning)
{
    initSDL();
    initOpenGL();

    SDL_Event e;
    while (appRunning.load()) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)                                   appRunning.store(false);
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) appRunning.store(false);
        }
        drawFrame(m_state.read());
        SDL_GL_SwapWindow(m_window);
    }
    shutdown();
}

void Renderer::initSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    m_window = SDL_CreateWindow(
        "DJ Visualizer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWindowW, kWindowH,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_window) throw std::runtime_error(SDL_GetError());

    m_glContext = SDL_GL_CreateContext(m_window);
    SDL_GL_SetSwapInterval(0); // vsync OFF — we drive our own frame pacing
}

void Renderer::initOpenGL()
{
    // If using GLAD, call gladLoadGLLoader here before any gl* calls.
    m_shaderProgram = loadShaderProgram("shaders/2d/spectrum.vert",
                                        "shaders/2d/spectrum.frag");
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // Reserve: kSpectrumBins bars * 6 vertices (2 triangles) * 2 floats (x,y)
    glBufferData(GL_ARRAY_BUFFER,
                 kSpectrumBins * 6 * 2 * sizeof(float),
                 nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
}

void Renderer::drawFrame(const AudioState& s)
{
    glClearColor(0.05f, 0.05f, 0.05f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // TODO: build bar geometry from s.spectrum[], upload, draw.
    // Each bin i → one quad scaled by s.spectrum[i].
    // On beat: s.onBeat == true → flash, strobe, scale effect, etc.
    (void)s;

    glUseProgram(m_shaderProgram);
    glBindVertexArray(m_vao);
    // glDrawArrays(GL_TRIANGLES, 0, kSpectrumBins * 6);
}

void Renderer::shutdown()
{
    if (m_vbo)          { glDeleteBuffers(1, &m_vbo);         m_vbo = 0; }
    if (m_vao)          { glDeleteVertexArrays(1, &m_vao);    m_vao = 0; }
    if (m_shaderProgram){ glDeleteProgram(m_shaderProgram);   m_shaderProgram = 0; }
    if (m_glContext)    { SDL_GL_DeleteContext(m_glContext);   m_glContext = nullptr; }
    if (m_window)       { SDL_DestroyWindow(m_window);        m_window = nullptr; }
    SDL_Quit();
}

} // namespace dv
