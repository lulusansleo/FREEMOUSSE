#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

uniform vec2  uCenter;   // NDC centre
uniform vec2  uScale;    // NDC half-extents (accounts for squash/stretch)

void main()
{
    vUV = aUV;
    gl_Position = vec4(uCenter + aPos * uScale, 0.0, 1.0);
}
