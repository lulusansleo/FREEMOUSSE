#include "render/shader.hpp"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL_opengl.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace dv {

static std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open shader: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static unsigned int compileShader(GLenum type, const std::string& src)
{
    unsigned int id = glCreateShader(type);
    const char*  c  = src.c_str();
    glShaderSource(id, 1, &c, nullptr);
    glCompileShader(id);

    int ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        throw std::runtime_error(std::string("Shader compile error:\n") + log);
    }
    return id;
}

unsigned int loadShaderProgram(const std::string& vertPath,
                               const std::string& fragPath)
{
    unsigned int vert = compileShader(GL_VERTEX_SHADER,   readFile(vertPath));
    unsigned int frag = compileShader(GL_FRAGMENT_SHADER, readFile(fragPath));
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    glDeleteShader(vert);
    glDeleteShader(frag);

    int ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        throw std::runtime_error(std::string("Shader link error:\n") + log);
    }
    return prog;
}

} // namespace dv
