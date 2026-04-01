#version 330 core

in  vec3 vWorldPos;
in  vec3 vNormal;
out vec4 fragColor;

uniform vec3 uColor;
uniform vec3 uLightPos; // key light (world space)
uniform vec3 uCamPos;   // camera (world space)

void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uCamPos   - vWorldPos);
    vec3 H = normalize(L + V);

    float ambient  = 0.22;
    float diffuse  = max(0.0, dot(N, L)) * 0.72;
    float specular = pow(max(0.0, dot(N, H)), 38.0) * 0.38;

    vec3 lit = uColor * (ambient + diffuse) + vec3(specular);
    fragColor = vec4(lit, 1.0);
}
