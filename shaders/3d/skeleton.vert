#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat; // transpose(inverse(mat3(uModel)))

out vec3 vWorldPos;
out vec3 vNormal;

void main()
{
    vWorldPos   = vec3(uModel * vec4(aPos, 1.0));
    vNormal     = uNormalMat * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
