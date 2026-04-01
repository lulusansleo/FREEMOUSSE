#version 330 core

layout(location = 0) in vec2 aPos;   // unit quad [-1..1]
layout(location = 1) in vec2 aUV;    // UV [0..1]

// Bone world matrix — turns bone-local space into NDC
uniform mat4 uBone;
// NDC half-extents of this sprite part (width, height)
uniform vec2 uHalfSize;

out vec2 vUV;

void main()
{
    // Scale the unit quad to the sprite's NDC size, then apply the bone
    // world transform (which includes all ancestor rotations/translations).
    gl_Position = uBone * vec4(aPos * uHalfSize, 0.0, 1.0);
    vUV         = aUV;
}
