#include "render/logo.hpp"

#include "render/shader.hpp"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL_opengl.h>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace dv {

static constexpr float kAspect = 1280.f / 720.f;

Logo::~Logo() {
    shutdownGL();
}

void Logo::load(const std::string& path) {
    int channels;
    unsigned char* data = stbi_load(path.c_str(), &m_w, &m_h, &channels, 4);
    if (!data)
        throw std::runtime_error("Logo: failed to load " + path);

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_w, m_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    initGL();
}

void Logo::draw(float scaleX, float scaleY, float alpha) {
    if (!m_tex || !m_shader)
        return;

    // Logo size in NDC: ~28% of screen height, corrected for aspect
    const float ndcH = 0.50f;
    const float ndcW = ndcH / kAspect;

    glUseProgram(m_shader);
    glUniform2f(glGetUniformLocation(m_shader, "uCenter"), 0.f, 0.f);
    glUniform2f(glGetUniformLocation(m_shader, "uScale"), ndcW * scaleX, ndcH * scaleY);
    glUniform1f(glGetUniformLocation(m_shader, "uAlpha"), alpha);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glUniform1i(glGetUniformLocation(m_shader, "uTex"), 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void Logo::initGL() {
    m_shader = loadShaderProgram("shaders/2d/logo.vert", "shaders/2d/logo.frag");

    // Quad: position (xy) + UV (uv)
    static const float quad[] = {
        -1.f, -1.f, 0.f, 1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0.f, -1.f, 1.f, 0.f, 0.f,
    };
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    // aPos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // aUV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Logo::shutdownGL() {
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_shader) {
        glDeleteProgram(m_shader);
        m_shader = 0;
    }
    if (m_tex) {
        glDeleteTextures(1, &m_tex);
        m_tex = 0;
    }
}

} // namespace dv
