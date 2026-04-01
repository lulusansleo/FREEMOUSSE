#version 330 core

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform float     uAlpha;

void main()
{
    vec4 tex = texture(uTex, vUV);
    // Multiply alpha so the logo respects transparency in the PNG
    fragColor = vec4(tex.rgb, tex.a * uAlpha);
}
