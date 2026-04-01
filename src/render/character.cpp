#include "render/character.hpp"

#include "render/shader.hpp"

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL_opengl.h>

// stb_image.h is already *defined* (STB_IMAGE_IMPLEMENTATION) in logo.cpp;
// include here for declarations only.
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace dv {

static constexpr float kAspect = 1280.f / 720.f;

Character::~Character() {
    shutdownGL();
}

// ── load ──────────────────────────────────────────────────────────────────

void Character::load(const std::string& texPath) {
    int channels;
    unsigned char* data = stbi_load(texPath.c_str(), &m_texW, &m_texH, &channels, 4);
    if (!data)
        throw std::runtime_error("Character: failed to load " + texPath);

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_texW, m_texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    buildSkeleton();
    initGL();
}

// ── buildSkeleton ─────────────────────────────────────────────────────────
//
// Default setup: one root bone, one sprite part covering the full image.
//
// When you have a sliced character, replace (or extend) this function:
//
//   m_bones = {
//     {"root",      -1, {0.f,  0.f}},
//     {"torso",      0, {0.f,  0.f}},
//     {"head",       1, {0.f,  0.22f}},
//     {"left_arm",   1, {-0.18f, 0.05f}},
//     {"right_arm",  1, { 0.18f, 0.05f}},
//     {"left_leg",   1, {-0.08f,-0.22f}},
//     {"right_leg",  1, { 0.08f,-0.22f}},
//   };
//
//   m_parts = {
//     // boneIdx, uvMin, uvMax, ndcHalfSize, zOrder
//     {1, {0.25f,0.10f}, {0.75f,0.55f}, {torsoW, torsoH}, 1},
//     {2, {0.30f,0.00f}, {0.70f,0.12f}, {headW,  headH},  2},
//     // ...
//   };

void Character::buildSkeleton() {
    // Preserve the texture's own aspect ratio so the image is never stretched.
    float texAspect = (float)m_texW / (float)m_texH;
    float halfH     = 0.50f;
    float halfW     = halfH * texAspect / kAspect;

    m_bones = {
        {"root", -1, {0.f, 0.f}, 0.f, {1.f, 1.f}, glm::mat4(1.f)},
    };

    m_parts = {
        {0, {0.f, 0.f}, {1.f, 1.f}, {halfW, halfH}, 0},
    };
}

// ── update ────────────────────────────────────────────────────────────────

void Character::update(float dt, const AudioState& s, float t) {
    // Beat pulse: instantly 1 on beat, then decays to 0 over ~0.18 s.
    if (s.onBeat)
        m_beatPulse = 1.f;
    m_beatPulse = std::max(0.f, m_beatPulse - dt * 5.5f);

    float bpm      = std::max(60.f, s.bpm);
    float beatFreq = bpm / 60.f;            // beats per second
    float rmsClamp = std::min(1.f, s.rms * 1.5f);

    // ── Root bone ─────────────────────────────────────────────────────────

    // Beat squash-and-bounce
    //   p=1: max squash (hit frame), scaleY low, scaleX wide
    //   p=0.5: parabola peak → max upward bounce, squash fading
    //   p=0: fully settled
    float p         = m_beatPulse;
    float squashAmt = 0.20f * std::max(0.3f, rmsClamp);
    float squashY   = 1.f - p * squashAmt;
    float squashX   = 1.f + p * squashAmt * 0.65f;
    float bounceY   = 4.f * p * (1.f - p) * 0.07f;  // parabola: 0 at p=0,1; peak at p=0.5

    // Continuous body sway at half the beat frequency (left ↔ right)
    float swayPhase = t * beatFreq * glm::pi<float>() * 0.5f;
    float sway      = std::sin(swayPhase);
    float swayAngle = sway * 0.055f;              // ±~3 degrees
    float swayX     = sway * 0.012f;              // subtle lateral shift

    auto& root  = m_bones[0];
    root.pos    = {swayX, bounceY};
    root.rot    = swayAngle;
    root.scale  = {squashX, squashY};

    // ── Future bones (head, arms, legs…) ─────────────────────────────────
    // Example: make head nod slightly opposite the body sway
    //   if (m_bones.size() > 2) {
    //       auto& head = m_bones[2];
    //       head.rot   = -swayAngle * 0.5f + p * 0.08f;
    //   }
    // Example: arms flap on beat
    //   if (m_bones.size() > 3) {
    //       m_bones[3].rot =  sway * 0.3f - p * 0.4f;  // left arm
    //       m_bones[4].rot = -sway * 0.3f + p * 0.4f;  // right arm
    //   }

    computeWorldMatrices();
}

// ── draw ──────────────────────────────────────────────────────────────────

void Character::draw() {
    if (!m_tex || !m_shader || m_parts.empty())
        return;

    // Sort parts back-to-front by zOrder
    std::vector<const SpritePart*> sorted;
    sorted.reserve(m_parts.size());
    for (const auto& p : m_parts)
        sorted.push_back(&p);
    std::sort(sorted.begin(), sorted.end(),
              [](const SpritePart* a, const SpritePart* b) { return a->zOrder < b->zOrder; });

    glUseProgram(m_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glUniform1i(glGetUniformLocation(m_shader, "uTex"), 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(m_vao);

    for (const auto* part : sorted)
        drawPart(*part, 1.0f);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

// ── Private ───────────────────────────────────────────────────────────────

void Character::computeWorldMatrices() {
    // Bones must be stored in parent-before-child order.
    for (auto& bone : m_bones) {
        glm::mat4 local = glm::mat4(1.f);
        local = glm::translate(local, glm::vec3(bone.pos, 0.f));
        local = glm::rotate(local, bone.rot, glm::vec3(0.f, 0.f, 1.f));
        local = glm::scale(local, glm::vec3(bone.scale, 1.f));

        bone.world = (bone.parent < 0) ? local
                                       : m_bones[bone.parent].world * local;
    }
}

void Character::drawPart(const SpritePart& part, float globalAlpha) const {
    const auto& bone = m_bones[part.boneIdx];
    glUniformMatrix4fv(glGetUniformLocation(m_shader, "uBone"),
                       1, GL_FALSE, glm::value_ptr(bone.world));
    glUniform2f(glGetUniformLocation(m_shader, "uHalfSize"),
                part.ndcHalfSize.x, part.ndcHalfSize.y);
    glUniform1f(glGetUniformLocation(m_shader, "uAlpha"), globalAlpha);
    glUniform2f(glGetUniformLocation(m_shader, "uUVMin"),
                part.uvMin.x, part.uvMin.y);
    glUniform2f(glGetUniformLocation(m_shader, "uUVMax"),
                part.uvMax.x, part.uvMax.y);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Character::initGL() {
    m_shader = loadShaderProgram("shaders/2d/character.vert", "shaders/2d/character.frag");

    // Unit quad: position (xy) + UV (uv) interleaved
    static const float quad[] = {
        -1.f, -1.f,  0.f, 1.f,
         1.f, -1.f,  1.f, 1.f,
         1.f,  1.f,  1.f, 0.f,
        -1.f,  1.f,  0.f, 0.f,
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    // location 0: aPos (vec2)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // location 1: aUV (vec2)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Character::shutdownGL() {
    if (m_vbo)    { glDeleteBuffers(1, &m_vbo);        m_vbo    = 0; }
    if (m_vao)    { glDeleteVertexArrays(1, &m_vao);   m_vao    = 0; }
    if (m_shader) { glDeleteProgram(m_shader);          m_shader = 0; }
    if (m_tex)    { glDeleteTextures(1, &m_tex);        m_tex    = 0; }
}

} // namespace dv
