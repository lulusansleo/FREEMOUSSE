#pragma once
#include <string>

namespace dv {

// Load, compile and link a GLSL program from two source files.
unsigned int loadShaderProgram(const std::string& vertPath, const std::string& fragPath);

} // namespace dv
