#pragma once
#include "core/audio_state.hpp"

#include <glm/glm.hpp>
#include <random>
#include <vector>

namespace dv {

struct Bubble {
    glm::vec2 pos;
    glm::vec2 vel;
    float radius;
    float alpha;
    float targetAlpha;
    float hueOffset; // [0..1] seed, kept in FM blue family in shader
    float shimmerPhase;
    float age;
    float lifetime;
    float wobble;
    bool popping{false};
    float popScale{1.f};
    float variant{0.f}; // 0=classic, 1=frosted, 2=deep-glow, 3=warm
};

// Matches shader layout (locations 1–6)
struct BubbleInstance {
    glm::vec2 centre;
    float radius;
    float alpha;
    float hueOffset;
    float time;
    float variant;
};

class BubbleSystem {
  public:
    BubbleSystem();
    ~BubbleSystem();

    void init(); // call after GL context
    void update(float dt, const AudioState& s, float t);
    void draw(float t);

    static constexpr int kMaxBubbles = 420;

  private:
    void spawn(bool onBeat, float rms);
    void initGL();
    void shutdownGL();

    std::vector<Bubble> m_bubbles;
    unsigned int m_shader{0};
    unsigned int m_quadVAO{0};
    unsigned int m_quadVBO{0};
    unsigned int m_instVBO{0};
    float m_spawnAccum{0.f};
    float m_lastBeatTime{-999.f};
    std::mt19937 m_rng{std::random_device{}()};
    std::uniform_real_distribution<float> m_rand{0.f, 1.f};
};

} // namespace dv
