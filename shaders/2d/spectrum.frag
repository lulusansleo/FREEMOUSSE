#version 330 core

in  float vMag;
out vec4  fragColor;

void main()
{
    // Bass (low vMag) = warm orange-red, treble (high vMag) = cool blue
    vec3 col = mix(vec3(1.0, 0.3, 0.1), vec3(0.1, 0.6, 1.0), vMag);
    fragColor = vec4(col, 1.0);
}
