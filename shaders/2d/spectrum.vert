#version 330 core

// Each vertex: x = normalized screen position [0..1],
//              y = magnitude (bar height) [0..1]
layout(location = 0) in vec2 aPos;

out float vMag;

void main()
{
    // Map to clip space [-1..1]
    gl_Position = vec4(aPos.x * 2.0 - 1.0, aPos.y * 2.0 - 1.0, 0.0, 1.0);
    vMag = aPos.y;
}
