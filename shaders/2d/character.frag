#version 330 core

in  vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform float     uAlpha;
// UV sub-region within the texture atlas ([0..1] range for full texture)
uniform vec2 uUVMin;
uniform vec2 uUVMax;

void main()
{
    vec2 uv      = uUVMin + vUV * (uUVMax - uUVMin);
    vec4 texCol  = texture(uTex, uv);
    fragColor    = vec4(texCol.rgb, texCol.a * uAlpha);
}
