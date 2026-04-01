#version 330 core

layout(location = 0) in vec2 aPos;

layout(location = 1) in vec2  iCenter;
layout(location = 2) in float iRadius;
layout(location = 3) in float iAlpha;
layout(location = 4) in float iHueOffset;
layout(location = 5) in float iTime;
layout(location = 6) in float iVariant;

out vec2  vLocalPos;
out float vAlpha;
out float vHueOffset;
out float vTime;
out float vVariant;

void main()
{
    vLocalPos  = aPos;
    vAlpha     = iAlpha;
    vHueOffset = iHueOffset;
    vTime      = iTime;
    vVariant   = iVariant;
    gl_Position = vec4(iCenter + aPos * iRadius, 0.0, 1.0);
}
