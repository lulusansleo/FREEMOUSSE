#include "render/skinned_character.hpp"
#include "render/shader.hpp"

// tinygltf — define IMPLEMENTATION in exactly this TU.
// Exclude stb_image (already implemented in logo.cpp).
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL_opengl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace dv {

// ── Destructor ────────────────────────────────────────────────────────────

SkinnedCharacter::~SkinnedCharacter() {
    shutdownGL();
}

// ── Accessor helpers (anonymous namespace) ────────────────────────────────

namespace {

const uint8_t* accessorBase(const tinygltf::Model& m, int idx) {
    const auto& acc = m.accessors[idx];
    const auto& bv  = m.bufferViews[acc.bufferView];
    return m.buffers[bv.buffer].data.data() + bv.byteOffset + acc.byteOffset;
}

int accessorStride(const tinygltf::Model& m, int idx, int defaultStride) {
    const auto& bv = m.bufferViews[m.accessors[idx].bufferView];
    return bv.byteStride ? bv.byteStride : defaultStride;
}

void readVec3s(const tinygltf::Model& m, int idx, std::vector<glm::vec3>& out) {
    const auto& acc = m.accessors[idx];
    const uint8_t* p = accessorBase(m, idx);
    int stride = accessorStride(m, idx, 12);
    out.resize(acc.count);
    for (size_t i = 0; i < acc.count; i++)
        std::memcpy(&out[i], p + i * stride, 12);
}

void readVec4s(const tinygltf::Model& m, int idx, std::vector<glm::vec4>& out) {
    const auto& acc = m.accessors[idx];
    const uint8_t* p = accessorBase(m, idx);
    int stride = accessorStride(m, idx, 16);
    out.resize(acc.count);
    for (size_t i = 0; i < acc.count; i++)
        std::memcpy(&out[i], p + i * stride, 16);
}

void readMat4s(const tinygltf::Model& m, int idx, std::vector<glm::mat4>& out) {
    const auto& acc = m.accessors[idx];
    const uint8_t* p = accessorBase(m, idx);
    int stride = accessorStride(m, idx, 64);
    out.resize(acc.count);
    for (size_t i = 0; i < acc.count; i++)
        std::memcpy(&out[i], p + i * stride, 64);
}

void readFloats(const tinygltf::Model& m, int idx, std::vector<float>& out) {
    const auto& acc = m.accessors[idx];
    const uint8_t* p = accessorBase(m, idx);
    int stride = accessorStride(m, idx, 4);
    out.resize(acc.count);
    for (size_t i = 0; i < acc.count; i++)
        std::memcpy(&out[i], p + i * stride, 4);
}

// JOINTS_0 can be UNSIGNED_BYTE or UNSIGNED_SHORT — always output float vec4
void readJoints(const tinygltf::Model& m, int idx, std::vector<glm::vec4>& out) {
    const auto& acc = m.accessors[idx];
    const uint8_t* p = accessorBase(m, idx);
    out.resize(acc.count);

    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        int stride = accessorStride(m, idx, 8);
        for (size_t i = 0; i < acc.count; i++) {
            uint16_t j[4];
            std::memcpy(j, p + i * stride, 8);
            out[i] = {(float)j[0], (float)j[1], (float)j[2], (float)j[3]};
        }
    } else { // UNSIGNED_BYTE
        int stride = accessorStride(m, idx, 4);
        for (size_t i = 0; i < acc.count; i++) {
            const uint8_t* j = p + i * stride;
            out[i] = {(float)j[0], (float)j[1], (float)j[2], (float)j[3]};
        }
    }
}

void readIndices(const tinygltf::Model& m, int idx, std::vector<uint32_t>& out) {
    const auto& acc = m.accessors[idx];
    const uint8_t* p = accessorBase(m, idx);
    out.resize(acc.count);

    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        for (size_t i = 0; i < acc.count; i++) std::memcpy(&out[i], p + i*4, 4);
    } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        for (size_t i = 0; i < acc.count; i++) {
            uint16_t v; std::memcpy(&v, p + i*2, 2);
            out[i] = v;
        }
    } else {
        for (size_t i = 0; i < acc.count; i++) out[i] = p[i];
    }
}

// Build TRS from a glTF node (handles both matrix and TRS forms)
void nodeTRS(const tinygltf::Node& n,
             glm::vec3& t, glm::quat& r, glm::vec3& s) {
    t = {0.f, 0.f, 0.f};
    r = {1.f, 0.f, 0.f, 0.f}; // identity: w=1
    s = {1.f, 1.f, 1.f};

    if (n.matrix.size() == 16) {
        glm::mat4 mat;
        for (int i = 0; i < 16; i++) mat[i/4][i%4] = (float)n.matrix[i];
        glm::vec3 skew; glm::vec4 persp;
        glm::decompose(mat, s, r, t, skew, persp);
        r = glm::conjugate(r); // glm::decompose returns conjugate
    } else {
        if (n.translation.size() == 3)
            t = {(float)n.translation[0], (float)n.translation[1], (float)n.translation[2]};
        if (n.rotation.size() == 4) // glTF: xyzw
            r = glm::quat((float)n.rotation[3], (float)n.rotation[0],
                          (float)n.rotation[1], (float)n.rotation[2]);
        if (n.scale.size() == 3)
            s = {(float)n.scale[0], (float)n.scale[1], (float)n.scale[2]};
    }
}

} // anonymous namespace

// ── load ──────────────────────────────────────────────────────────────────

void SkinnedCharacter::load(const std::string& glbPath) {
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;

    // Set a no-op image loader since we don't need textures
    loader.SetImageLoader(
        [](tinygltf::Image*, const int, std::string*, std::string*,
           int, int, const unsigned char*, int, void*) { return true; },
        nullptr);

    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, glbPath);
    if (!ok)
        throw std::runtime_error("SkinnedCharacter: " + err);

    parseNodes(model);
    parseSkin(model);
    parseMesh(model);
    parseAnimations(model);
    fitCamera();
    initGL();
}

// ── parseNodes ────────────────────────────────────────────────────────────

void SkinnedCharacter::parseNodes(const tinygltf::Model& model) {
    int n = (int)model.nodes.size();
    m_nodes.resize(n);
    m_poses.resize(n);
    m_nodeWorldMats.assign(n, glm::mat4(1.f));

    for (int i = 0; i < n; i++) {
        const auto& gn = model.nodes[i];
        m_nodes[i].name     = gn.name;
        m_nodes[i].children.assign(gn.children.begin(), gn.children.end());
        nodeTRS(gn, m_nodes[i].restT, m_nodes[i].restR, m_nodes[i].restS);
    }

    // Set parent info (glTF stores children, not parents)
    for (int i = 0; i < n; i++)
        for (int c : m_nodes[i].children)
            m_nodes[c].parent = i;

    // Collect scene root nodes
    for (int i = 0; i < n; i++)
        if (m_nodes[i].parent == -1)
            m_sceneRoots.push_back(i);

    // Find hip node for beat reactivity (Mixamo names it "Hips")
    for (int i = 0; i < n; i++) {
        const auto& nm = m_nodes[i].name;
        if (nm.find("Hips") != std::string::npos ||
            nm.find("hips") != std::string::npos ||
            nm.find("Pelvis") != std::string::npos) {
            m_hipNodeIdx = i;
            break;
        }
    }
}

// ── parseSkin ─────────────────────────────────────────────────────────────

void SkinnedCharacter::parseSkin(const tinygltf::Model& model) {
    if (model.skins.empty()) return;
    const auto& skin = model.skins[0];

    m_joints.assign(skin.joints.begin(), skin.joints.end());

    if (skin.inverseBindMatrices >= 0)
        readMat4s(model, skin.inverseBindMatrices, m_invBindMats);
    else
        m_invBindMats.assign(m_joints.size(), glm::mat4(1.f));

    m_boneMatrices.assign(m_joints.size(), glm::mat4(1.f));
}

// ── parseMesh ─────────────────────────────────────────────────────────────

void SkinnedCharacter::parseMesh(const tinygltf::Model& model) {
    for (const auto& node : model.nodes) {
        if (node.mesh < 0) continue;
        for (const auto& prim : model.meshes[node.mesh].primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES &&
                prim.mode != -1 /* default = triangles */) continue;

            uint32_t baseVtx = (uint32_t)m_vertices.size();

            // Positions (required)
            auto it = prim.attributes.find("POSITION");
            if (it == prim.attributes.end()) continue;
            std::vector<glm::vec3> positions;
            readVec3s(model, it->second, positions);

            // Normals
            std::vector<glm::vec3> normals(positions.size(), {0.f, 1.f, 0.f});
            it = prim.attributes.find("NORMAL");
            if (it != prim.attributes.end())
                readVec3s(model, it->second, normals);

            // Joints (bone indices)
            std::vector<glm::vec4> joints(positions.size(), {0.f, 0.f, 0.f, 0.f});
            it = prim.attributes.find("JOINTS_0");
            if (it != prim.attributes.end())
                readJoints(model, it->second, joints);

            // Weights
            std::vector<glm::vec4> weights(positions.size(), {1.f, 0.f, 0.f, 0.f});
            it = prim.attributes.find("WEIGHTS_0");
            if (it != prim.attributes.end())
                readVec4s(model, it->second, weights);

            for (int i = 0; i < (int)positions.size(); i++)
                m_vertices.push_back({positions[i], normals[i], joints[i], weights[i]});

            // Indices
            if (prim.indices >= 0) {
                std::vector<uint32_t> primIdx;
                readIndices(model, prim.indices, primIdx);
                for (uint32_t idx : primIdx)
                    m_indices.push_back(baseVtx + idx);
            } else {
                for (uint32_t i = 0; i < (uint32_t)positions.size(); i++)
                    m_indices.push_back(baseVtx + i);
            }
        }
    }
}

// ── parseAnimations ───────────────────────────────────────────────────────

void SkinnedCharacter::parseAnimations(const tinygltf::Model& model) {
    for (const auto& ganim : model.animations) {
        AnimClip clip;
        clip.name = ganim.name;

        for (const auto& ch : ganim.channels) {
            if (ch.target_path == "weights") continue; // morph targets unsupported

            AnimChannel channel;
            channel.nodeIdx = ch.target_node;

            if      (ch.target_path == "translation") channel.path = AnimPath::Translation;
            else if (ch.target_path == "rotation")    channel.path = AnimPath::Rotation;
            else if (ch.target_path == "scale")       channel.path = AnimPath::Scale;
            else continue;

            const auto& samp = ganim.samplers[ch.sampler];

            // Time keyframes
            readFloats(model, samp.input, channel.times);
            if (!channel.times.empty())
                clip.duration = std::max(clip.duration, channel.times.back());

            // Values
            if (channel.path == AnimPath::Rotation) {
                readVec4s(model, samp.output, channel.values); // xyzw
            } else {
                std::vector<glm::vec3> v3;
                readVec3s(model, samp.output, v3);
                channel.values.resize(v3.size());
                for (size_t i = 0; i < v3.size(); i++)
                    channel.values[i] = {v3[i], 0.f};
            }

            clip.channels.push_back(std::move(channel));
        }

        if (clip.duration > 0.f)
            m_clips.push_back(std::move(clip));
    }
}

// ── fitCamera ─────────────────────────────────────────────────────────────

void SkinnedCharacter::fitCamera() {
    if (m_vertices.empty()) return;

    glm::vec3 bmin(1e9f), bmax(-1e9f);
    for (const auto& v : m_vertices) {
        bmin = glm::min(bmin, v.pos);
        bmax = glm::max(bmax, v.pos);
    }

    glm::vec3 center = (bmin + bmax) * 0.5f;
    float     height = bmax.y - bmin.y;
    float     radius = glm::length(bmax - bmin) * 0.5f;
    float     dist   = radius * 2.2f;

    m_camPos = center + glm::vec3(0.f, height * 0.05f, dist);
    m_view   = glm::lookAt(m_camPos,
                           center + glm::vec3(0.f, height * 0.05f, 0.f),
                           glm::vec3(0.f, 1.f, 0.f));
    m_proj   = glm::perspective(glm::radians(45.f), 1280.f / 720.f,
                                dist * 0.01f, dist * 20.f);
}

// ── update ────────────────────────────────────────────────────────────────

void SkinnedCharacter::update(float dt, const AudioState& s, float t) {
    // Smooth energy
    m_energy += (s.rms - m_energy) * std::min(1.f, dt * 6.f);

    // Beat pulse: instant rise on beat, smooth decay
    if (s.onBeat) m_beatPulse = 1.f;
    m_beatPulse = std::max(0.f, m_beatPulse - dt * 5.5f);

    if (m_clips.empty()) return;

    // Advance animation — speed scales slightly with energy
    float speed    = 0.85f + m_energy * 0.30f;
    m_animTime     = std::fmod(m_animTime + dt * speed, m_clips[0].duration);

    evaluate(m_animTime);
}

// ── evaluate ──────────────────────────────────────────────────────────────

void SkinnedCharacter::evaluate(float animTime) {
    // 1. Reset to rest pose
    for (int i = 0; i < (int)m_nodes.size(); i++) {
        m_poses[i].t = m_nodes[i].restT;
        m_poses[i].r = m_nodes[i].restR;
        m_poses[i].s = m_nodes[i].restS;
    }

    // 2. Apply animation channels
    const auto& clip = m_clips[0];
    for (const auto& ch : clip.channels) {
        if (ch.nodeIdx < 0 || ch.nodeIdx >= (int)m_poses.size()) continue;
        if (ch.times.empty()) continue;

        // Find surrounding keyframes
        float t0 = ch.times.front(), tN = ch.times.back();
        float clamped = std::max(t0, std::min(tN, animTime));

        int k = 0;
        for (int i = 0; i + 1 < (int)ch.times.size(); i++) {
            if (clamped < ch.times[i + 1]) { k = i; break; }
            k = i;
        }
        int k1 = std::min(k + 1, (int)ch.values.size() - 1);
        float dt01 = ch.times[std::min(k+1,(int)ch.times.size()-1)] - ch.times[k];
        float alpha = (dt01 > 1e-6f) ? (clamped - ch.times[k]) / dt01 : 0.f;

        auto& pose = m_poses[ch.nodeIdx];
        if (ch.path == AnimPath::Translation) {
            pose.t = glm::mix(glm::vec3(ch.values[k]), glm::vec3(ch.values[k1]), alpha);
        } else if (ch.path == AnimPath::Rotation) {
            // glTF stores xyzw; GLM quat is (w,x,y,z)
            glm::quat q0(ch.values[k ].w, ch.values[k ].x, ch.values[k ].y, ch.values[k ].z);
            glm::quat q1(ch.values[k1].w, ch.values[k1].x, ch.values[k1].y, ch.values[k1].z);
            pose.r = glm::slerp(q0, q1, alpha);
        } else {
            pose.s = glm::mix(glm::vec3(ch.values[k]), glm::vec3(ch.values[k1]), alpha);
        }
    }

    // 3. Beat-reactive: bounce the hip up, squash on hit
    if (m_hipNodeIdx >= 0) {
        float p = m_beatPulse;
        m_poses[m_hipNodeIdx].t.y += 4.f * p * (1.f - p) * 0.06f; // parabolic lift
        float squash = 1.f - p * 0.07f;
        m_poses[m_hipNodeIdx].s.y *= squash;
        m_poses[m_hipNodeIdx].s.x *= (1.f + p * 0.04f);
        m_poses[m_hipNodeIdx].s.z *= (1.f + p * 0.04f);
    }

    // 4. Compute world matrices (depth-first from each scene root)
    for (int root : m_sceneRoots)
        computeWorldMatsRec(root, glm::mat4(1.f));

    // 5. Build bone matrices: nodeWorldMat * inverseBindMat
    for (int i = 0; i < (int)m_joints.size() && i < kMaxBones; i++)
        m_boneMatrices[i] = m_nodeWorldMats[m_joints[i]] * m_invBindMats[i];
}

void SkinnedCharacter::computeWorldMatsRec(int idx, const glm::mat4& parentMat) {
    const auto& pose = m_poses[idx];
    glm::mat4 local  = glm::translate(glm::mat4(1.f), pose.t)
                     * glm::mat4_cast(pose.r)
                     * glm::scale(glm::mat4(1.f), pose.s);
    m_nodeWorldMats[idx] = parentMat * local;
    for (int child : m_nodes[idx].children)
        computeWorldMatsRec(child, m_nodeWorldMats[idx]);
}

// ── draw ──────────────────────────────────────────────────────────────────

void SkinnedCharacter::draw() {
    if (!m_shader || !m_vao || m_indices.empty()) return;

    glEnable(GL_DEPTH_TEST);
    glUseProgram(m_shader);

    // Upload bone matrices (max kMaxBones)
    int boneCount = (int)std::min((size_t)kMaxBones, m_boneMatrices.size());
    glUniformMatrix4fv(m_uBones, boneCount, GL_FALSE,
                       glm::value_ptr(m_boneMatrices[0]));

    glUniformMatrix4fv(m_uProjView, 1, GL_FALSE,
                       glm::value_ptr(m_proj * m_view));
    glUniformMatrix4fv(m_uModel, 1, GL_FALSE,
                       glm::value_ptr(m_modelMat));

    static const glm::vec3 kLightPos(2.5f, 4.0f, 3.0f);
    static const glm::vec3 kColor(0.38f, 0.60f, 0.95f); // FM blue

    glUniform3fv(m_uLightPos, 1, glm::value_ptr(kLightPos));
    glUniform3fv(m_uCamPos,   1, glm::value_ptr(m_camPos));
    glUniform3fv(m_uColor,    1, glm::value_ptr(kColor));

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);
}

// ── GL setup / teardown ───────────────────────────────────────────────────

void SkinnedCharacter::initGL() {
    m_shader = loadShaderProgram("shaders/3d/skinned.vert", "shaders/3d/skinned.frag");

    m_uBones    = glGetUniformLocation(m_shader, "uBones");
    m_uProjView = glGetUniformLocation(m_shader, "uProjView");
    m_uModel    = glGetUniformLocation(m_shader, "uModel");
    m_uColor    = glGetUniformLocation(m_shader, "uColor");
    m_uLightPos = glGetUniformLocation(m_shader, "uLightPos");
    m_uCamPos   = glGetUniformLocation(m_shader, "uCamPos");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(m_vertices.size() * sizeof(SkinnedVertex)),
                 m_vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(m_indices.size() * sizeof(uint32_t)),
                 m_indices.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = sizeof(SkinnedVertex);
    // location 0: pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(SkinnedVertex, pos));
    glEnableVertexAttribArray(0);
    // location 1: normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(SkinnedVertex, normal));
    glEnableVertexAttribArray(1);
    // location 2: joints
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(SkinnedVertex, joints));
    glEnableVertexAttribArray(2);
    // location 3: weights
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                          (void*)offsetof(SkinnedVertex, weights));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

void SkinnedCharacter::shutdownGL() {
    if (m_ebo)    { glDeleteBuffers(1, &m_ebo);       m_ebo    = 0; }
    if (m_vbo)    { glDeleteBuffers(1, &m_vbo);       m_vbo    = 0; }
    if (m_vao)    { glDeleteVertexArrays(1, &m_vao);  m_vao    = 0; }
    if (m_shader) { glDeleteProgram(m_shader);         m_shader = 0; }
}

} // namespace dv
