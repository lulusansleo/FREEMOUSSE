#include "render/bubble_system.hpp"

#include "render/shader.hpp"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dv {

// Window aspect ratio — used to keep bubbles circular in NDC space
static constexpr float kAspect = 1280.f / 720.f;

BubbleSystem::BubbleSystem() = default;
BubbleSystem::~BubbleSystem() {
    shutdownGL();
}

void BubbleSystem::init() {
    m_bubbles.reserve(kMaxBubbles);
    initGL();
}

// ── Update ────────────────────────────────────────────────────────────────

void BubbleSystem::update(float dt, const AudioState& s, float t) {
    // Beat burst
    const bool onBeat = s.onBeat && (t - m_lastBeatTime > 0.08f);
    if (onBeat) {
        m_lastBeatTime = t;
        int burst = 4 + static_cast<int>(s.rms * 10.f);
        for (int i = 0; i < burst && (int)m_bubbles.size() < kMaxBubbles; ++i)
            spawn(true, s.rms);
    }

    // Continuous trickle — rate scales with RMS
    m_spawnAccum += dt;
    const float rate = 1.8f + s.rms * 5.2f; // bubbles / second
    const float period = 1.f / rate;
    while (m_spawnAccum >= period && (int)m_bubbles.size() < kMaxBubbles) {
        m_spawnAccum -= period;
        spawn(false, s.rms);
    }

    // Integrate
    for (auto& b : m_bubbles) {
        if (b.popping) {
            b.popScale += dt * 5.f;
            b.alpha -= dt * 3.5f;
            continue;
        }
        b.age += dt;
        b.wobble += dt * 1.9f;
        b.shimmerPhase += dt;
        b.pos += b.vel * dt;
        b.pos.x += std::sin(b.wobble) * 0.003f;
        b.alpha += (b.targetAlpha - b.alpha) * std::min(1.f, dt * 4.f);

        if (onBeat) {
            b.vel.y -= s.rms * 0.045f;
            b.vel.x += (m_rand(m_rng) - 0.5f) * 0.02f;
        }
        if (b.age >= b.lifetime)
            b.popping = true;
    }

    // Cull dead / off-screen
    m_bubbles.erase(std::remove_if(m_bubbles.begin(), m_bubbles.end(),
                                   [](const Bubble& b) {
                                       return (b.popping && b.alpha <= 0.f) || b.pos.y < -1.4f;
                                   }),
                    m_bubbles.end());
}

// ── Draw ──────────────────────────────────────────────────────────────────

void BubbleSystem::draw(float t) {
    if (m_bubbles.empty())
        return;

    std::vector<BubbleInstance> inst;
    inst.reserve(m_bubbles.size());
    for (const auto& b : m_bubbles) {
        if (b.alpha <= 0.f)
            continue;
        inst.push_back({b.pos, b.radius * b.popScale, std::max(0.f, b.alpha), b.hueOffset,
                        t + b.shimmerPhase});
    }
    if (inst.empty())
        return;

    glBindBuffer(GL_ARRAY_BUFFER, m_instVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(inst.size() * sizeof(BubbleInstance)),
                    inst.data());

    glUseProgram(m_shader);
    glBindVertexArray(m_quadVAO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, (GLsizei)inst.size());
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

// ── Private ───────────────────────────────────────────────────────────────

void BubbleSystem::spawn(bool onBeat, float rms) {
    Bubble b;
    // Spawn from the character's hands area (lower-centre of screen)
    b.pos = {(m_rand(m_rng) - 0.5f) * 0.6f, -0.15f + (m_rand(m_rng) - 0.5f) * 0.15f};
    float speed = (0.18f + m_rand(m_rng) * 0.28f) * (onBeat ? 1.7f : 1.f);
    b.vel = {(m_rand(m_rng) - 0.5f) * 0.04f, speed};
    // Radius in NDC — divide by aspect to stay circular
    float rPx = 0.022f + m_rand(m_rng) * 0.058f;
    b.radius = rPx / kAspect;
    b.alpha = 0.f;
    b.targetAlpha = 0.5f + m_rand(m_rng) * 0.35f;
    // Hue offset [0..1] — shader maps this into the FM blue family
    b.hueOffset = m_rand(m_rng);
    b.shimmerPhase = m_rand(m_rng) * 6.28f;
    b.wobble = m_rand(m_rng) * 6.28f;
    b.age = 0.f;
    b.lifetime = 4.f + m_rand(m_rng) * 3.5f;
    b.popping = false;
    b.popScale = 1.f;
    m_bubbles.push_back(b);
}

void BubbleSystem::initGL() {
    m_shader = loadShaderProgram("shaders/2d/bubble.vert", "shaders/2d/bubble.frag");

    // Unit quad: four corners of a square, used as a billboard per bubble
    static const float quad[] = {
        -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glGenBuffers(1, &m_instVBO);

    glBindVertexArray(m_quadVAO);

    // Quad vertices (location 0)
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    // Instance buffer (locations 1–5)
    glBindBuffer(GL_ARRAY_BUFFER, m_instVBO);
    glBufferData(GL_ARRAY_BUFFER, kMaxBubbles * sizeof(BubbleInstance), nullptr, GL_DYNAMIC_DRAW);

    const GLsizei stride = sizeof(BubbleInstance);

    // location 1: centre (vec2)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(BubbleInstance, centre));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    // location 2: radius (float)
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(BubbleInstance, radius));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    // location 3: alpha (float)
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BubbleInstance, alpha));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    // location 4: hueOffset (float)
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(BubbleInstance, hueOffset));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);

    // location 5: time (float)
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(BubbleInstance, time));
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

void BubbleSystem::shutdownGL() {
    if (m_instVBO) {
        glDeleteBuffers(1, &m_instVBO);
        m_instVBO = 0;
    }
    if (m_quadVBO) {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }
    if (m_quadVAO) {
        glDeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    if (m_shader) {
        glDeleteProgram(m_shader);
        m_shader = 0;
    }
}

} // namespace dv
