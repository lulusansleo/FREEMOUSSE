#pragma once
#include "core/audio_state.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

// Forward-declare so callers don't need to pull in all of tiny_gltf.h
namespace tinygltf { class Model; }

namespace dv {

static constexpr int kMaxBones = 100;

class SkinnedCharacter {
  public:
    SkinnedCharacter() = default;
    ~SkinnedCharacter();

    // Load a .glb file. Call after the OpenGL context is created.
    void load(const std::string& glbPath);

    // Drive animation + beat reactivity. Call every frame.
    void update(float dt, const AudioState& s, float t);

    // Draw with depth test. Caller is responsible for clearing depth if needed.
    void draw();

  private:
    // ── Internal data types ───────────────────────────────────────────────

    struct SkinnedVertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec4 joints;   // bone indices as floats (cast in shader)
        glm::vec4 weights;
    };

    struct NodeData {
        std::string      name;
        int              parent{-1};
        std::vector<int> children;
        // Rest-pose TRS (set from glTF node, used when no animation overrides)
        glm::vec3 restT{0.f};
        glm::quat restR{1.f, 0.f, 0.f, 0.f};
        glm::vec3 restS{1.f};
    };

    struct NodePose {
        glm::vec3 t{0.f};
        glm::quat r{1.f, 0.f, 0.f, 0.f};
        glm::vec3 s{1.f};
    };

    enum class AnimPath { Translation, Rotation, Scale };

    struct AnimChannel {
        int                    nodeIdx{0};
        AnimPath               path{AnimPath::Translation};
        std::vector<float>     times;
        // vec3 channels stored as vec4(x,y,z,0); rotation as vec4(x,y,z,w)
        std::vector<glm::vec4> values;
    };

    struct AnimClip {
        std::string              name;
        float                    duration{0.f};
        std::vector<AnimChannel> channels;
    };

    // ── Loading ───────────────────────────────────────────────────────────
    void parseNodes(const tinygltf::Model& model);
    void parseSkin(const tinygltf::Model& model);
    void parseMesh(const tinygltf::Model& model);
    void parseAnimations(const tinygltf::Model& model);
    void fitCamera();
    void initGL();
    void shutdownGL();

    // ── Per-frame evaluation ──────────────────────────────────────────────
    void evaluate(float animTime);
    void computeWorldMatsRec(int nodeIdx, const glm::mat4& parentMat);

    // ── Skeleton ──────────────────────────────────────────────────────────
    std::vector<NodeData>  m_nodes;
    std::vector<NodePose>  m_poses;          // current animated pose per node
    std::vector<glm::mat4> m_nodeWorldMats;
    std::vector<int>       m_joints;         // skin.joints: node indices
    std::vector<glm::mat4> m_invBindMats;
    std::vector<glm::mat4> m_boneMatrices;   // uploaded to shader each frame
    std::vector<int>       m_sceneRoots;
    int                    m_hipNodeIdx{-1}; // for beat-reactive bounce

    // ── Mesh ──────────────────────────────────────────────────────────────
    std::vector<SkinnedVertex> m_vertices;
    std::vector<uint32_t>      m_indices;

    // ── Animation ─────────────────────────────────────────────────────────
    std::vector<AnimClip> m_clips;
    float m_animTime{0.f};

    // ── Beat-reactive state ───────────────────────────────────────────────
    float m_beatPulse{0.f};
    float m_energy{0.f};

    // ── OpenGL ────────────────────────────────────────────────────────────
    unsigned int m_shader{0};
    unsigned int m_vao{0}, m_vbo{0}, m_ebo{0};
    // cached uniform locations
    int m_uBones{-1}, m_uProjView{-1}, m_uModel{-1};
    int m_uColor{-1}, m_uLightPos{-1}, m_uCamPos{-1};

    // ── Camera (owned here so the system is self-contained) ───────────────
    glm::mat4 m_proj{1.f};
    glm::mat4 m_view{1.f};
    glm::vec3 m_camPos{0.f, 1.5f, 3.5f};

    // Global model matrix — rotates/scales character if needed
    glm::mat4 m_modelMat{1.f};
};

} // namespace dv
