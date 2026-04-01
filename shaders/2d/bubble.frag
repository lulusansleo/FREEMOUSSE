#version 330 core

in vec2  vLocalPos;
in float vAlpha;
in float vHueOffset;
in float vTime;
in float vVariant;

out vec4 fragColor;

// Free Mousse palette helpers
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
    float dist = length(vLocalPos);
    if (dist > 1.0) discard;

    // ── 3-D sphere normal ──────────────────────────────────────────────────
    // Treat the unit disc as the projection of a sphere: the z-component of
    // the surface normal is the "depth" of the sphere at this pixel.
    float z      = sqrt(max(0.0, 1.0 - dist * dist));
    vec3  normal = normalize(vec3(vLocalPos, z));

    // Light from upper-left, toward the viewer
    vec3  lightDir = normalize(vec3(-0.4, 0.6, 1.0));
    vec3  viewDir  = vec3(0.0, 0.0, 1.0);
    vec3  halfVec  = normalize(lightDir + viewDir);
    float diffuse  = max(0.0, dot(normal, lightDir));
    float specular = pow(max(0.0, dot(normal, halfVec)), 48.0);

    // ── Iridescence hue base ───────────────────────────────────────────────
    float angle = atan(vLocalPos.y, vLocalPos.x);
    float hue   = vHueOffset
                + vTime  * 0.06
                + angle  * 0.12
                + dist   * 0.18;

    int   variant = int(vVariant + 0.5);
    vec3  col;
    float rim;
    float inner;

    // ── Variant 1 : Frosted / pearl ────────────────────────────────────────
    // Desaturated FM blue with strong diffuse scattering; looks like clouded
    // glass or a pearl.  Wider solid body, prominent specular.
    if (variant == 1) {
        float h   = 0.60 + fract(hue * 0.4) * 0.12;
        vec3  base = hsv2rgb(vec3(h, 0.40, 0.92));
        col   = mix(base, vec3(1.0), diffuse * 0.45 + specular * 0.40);
        rim   = smoothstep(0.40, 1.00, dist) * 1.2;
        inner = (1.0 - smoothstep(0.0, 0.85, dist)) * 0.55;

    // ── Variant 2 : Deep glow ─────────────────────────────────────────────
    // Dark sphere body with a very bright luminous rim — like a dark soap
    // film lit from behind.
    } else if (variant == 2) {
        float h1     = 0.62 + fract(hue)       * 0.13;
        float h2     = 0.55 + fract(hue + 0.5) * 0.20;
        float edgeMix = smoothstep(0.45, 0.95, dist);
        col   = mix(hsv2rgb(vec3(h1, 0.9, 0.25)),
                    hsv2rgb(vec3(h2, 0.6, 1.00)),
                    edgeMix);
        rim   = smoothstep(0.50, 1.00, dist) * 1.8;
        inner = (1.0 - smoothstep(0.0, 0.55, dist)) * 0.06;

    // ── Variant 3 : Warm sunset ────────────────────────────────────────────
    // Same thin-film structure but hues shifted into reds, oranges, pinks.
    } else if (variant == 3) {
        float base_h = vHueOffset * 0.15 + vTime * 0.04;
        float h1 = fract(base_h + 0.00); // red
        float h2 = fract(base_h + 0.06); // orange
        float h3 = fract(base_h + 0.92); // pink (wraps near 0)
        vec3 band1 = hsv2rgb(vec3(h1, 0.85, 0.95));
        vec3 band2 = hsv2rgb(vec3(h2, 0.80, 1.00));
        vec3 band3 = hsv2rgb(vec3(h3, 0.70, 0.90));
        float t1 = 0.5 + 0.5 * sin(dist * 6.28 + vTime * 1.4);
        float t2 = 0.5 + 0.5 * sin(dist * 4.71 - vTime * 1.1 + 1.0);
        col   = mix(mix(band1, band2, t1), band3, t2 * 0.5);
        rim   = smoothstep(0.55, 1.00, dist);
        inner = 1.0 - smoothstep(0.0,  0.65, dist);

    // ── Variant 0 : Classic FM soap bubble (default) ──────────────────────
    } else {
        float h1 = 0.55 + fract(hue)        * 0.20;
        float h2 = 0.55 + fract(hue + 0.33) * 0.20;
        float h3 = 0.55 + fract(hue + 0.66) * 0.20;
        vec3 band1 = hsv2rgb(vec3(h1, 0.75, 0.90));
        vec3 band2 = hsv2rgb(vec3(h2, 0.65, 0.95));
        vec3 band3 = hsv2rgb(vec3(h3, 0.55, 1.00));
        float t1 = 0.5 + 0.5 * sin(dist * 6.28 + vTime * 1.4);
        float t2 = 0.5 + 0.5 * sin(dist * 4.71 - vTime * 1.1 + 1.0);
        col   = mix(mix(band1, band2, t1), band3, t2 * 0.5);
        rim   = smoothstep(0.55, 1.00, dist);
        inner = 1.0 - smoothstep(0.0,  0.65, dist);
    }

    // ── 3-D specular highlight (replaces old fixed-position glint) ────────
    // Tint toward a warm-white at the highlight position; opacity is
    // independent of vAlpha so it stays visible even on faint bubbles.
    col   = mix(col, vec3(1.0, 1.0, 0.97), specular * 0.75);

    float alpha = vAlpha * (rim * 0.55 + inner * 0.07) + specular * 0.78;
    alpha      *= smoothstep(1.0, 0.96, dist);

    fragColor = vec4(col, alpha);
}
