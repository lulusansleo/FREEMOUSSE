#pragma once
#include "core/audio_state.hpp"

#include <string>

namespace dv {

// Loads a PNG with a transparent background and draws it as a beat-reactive
// billboard quad in the centre of the screen.
// Requires stb_image — add to third_party/ (single header, no build step).
class Logo {
  public:
    Logo() = default;
    ~Logo();

    // Call after GL context. path = e.g. "assets/freemousse_logo.png"
    void load(const std::string& path);

    // scaleX / scaleY come from Renderer's beat squash values
    void draw(float scaleX, float scaleY, float alpha = 1.f);

  private:
    void initGL();
    void shutdownGL();

    unsigned int m_shader{0};
    unsigned int m_vao{0};
    unsigned int m_vbo{0};
    unsigned int m_tex{0};
    int m_w{0}, m_h{0};
};

} // namespace dv
