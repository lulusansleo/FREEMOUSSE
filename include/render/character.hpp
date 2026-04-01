#pragma once
#include "core/audio_state.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace dv {

// ── Bone ──────────────────────────────────────────────────────────────────
// Stored in parent-before-child order so computeWorldMatrices() is a single
// forward pass.
struct Bone {
    std::string name;
    int         parent{-1};       // -1 = root
    glm::vec2   pos{0.f};         // local NDC position
    float       rot{0.f};         // local rotation (radians, CCW)
    glm::vec2   scale{1.f, 1.f};  // local scale
    glm::mat4   world{1.f};       // computed world matrix (do not set manually)
};

// ── SpritePart ────────────────────────────────────────────────────────────
// One textured quad that follows a bone.  uvMin/uvMax are in [0..1] texture
// space — use (0,0)→(1,1) for the full image, or sub-rects for sprite sheets.
// ndcSize is the half-extent in NDC (so total width = 2*ndcSize.x, etc.).
// zOrder controls back-to-front draw order (lower = drawn first).
struct SpritePart {
    int       boneIdx{0};
    glm::vec2 uvMin{0.f};
    glm::vec2 uvMax{1.f};
    glm::vec2 ndcHalfSize{0.35f, 0.35f};
    int       zOrder{0};
};

// ── Character ─────────────────────────────────────────────────────────────
// 2-D cut-out skeleton driven by procedural dance animation.
//
// HOW TO EXTEND FOR A SLICED CHARACTER
// ─────────────────────────────────────
// 1. Slice the PNG into parts (head, torso, left-arm, right-arm, legs…).
//    Either export separate images or keep them in a single atlas.
// 2. In buildSkeleton(), add one Bone per body segment with the correct
//    parent index and rest-pose NDC offset.
// 3. Add one SpritePart per bone pointing at the matching UV region.
// 4. In update(), drive each named bone's rot/pos to create dance motion.
class Character {
  public:
    Character() = default;
    ~Character();

    // Call after OpenGL context is created.
    // texPath  — same PNG / JPG already used for the logo.
    void load(const std::string& texPath);

    // Called every frame with delta-time, audio analysis, and running time.
    void update(float dt, const AudioState& s, float t);

    // Draw all sprite parts, back-to-front.
    void draw();

  private:
    // Build default single-part skeleton (whole image → root bone).
    void buildSkeleton();
    // Forward pass: propagate local transforms into world matrices.
    void computeWorldMatrices();

    void initGL();
    void shutdownGL();

    // Draw one sprite part using the bone's current world matrix.
    void drawPart(const SpritePart& part, float globalAlpha) const;

    std::vector<Bone>       m_bones;
    std::vector<SpritePart> m_parts;

    unsigned int m_shader{0};
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    unsigned int m_tex{0};
    int          m_texW{0}, m_texH{0};

    // Dance state
    float m_beatPulse{0.f};  // 1 on beat, decays to 0
};

} // namespace dv
