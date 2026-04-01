#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aJoints;   // bone indices (as floats, cast to int)
layout(location = 3) in vec4 aWeights;  // blend weights (must sum to 1)

// One matrix per joint: nodeWorldMat * inverseBindMat
uniform mat4 uBones[100];
uniform mat4 uProjView; // projection * view
uniform mat4 uModel;    // global model transform (scale / orientation fix)

out vec3 vWorldPos;
out vec3 vNormal;

void main()
{
    // Weighted blend of the four influencing bone transforms
    mat4 skin = aWeights.x * uBones[int(aJoints.x)]
              + aWeights.y * uBones[int(aJoints.y)]
              + aWeights.z * uBones[int(aJoints.z)]
              + aWeights.w * uBones[int(aJoints.w)];

    vec4 skinnedPos = skin * vec4(aPos, 1.0);
    vec4 worldPos   = uModel * skinnedPos;

    gl_Position = uProjView * worldPos;
    vWorldPos   = worldPos.xyz;
    // mat3(skin) is valid for normals when scale is uniform (true for most dance rigs)
    vNormal     = normalize(mat3(uModel) * mat3(skin) * aNormal);
}
