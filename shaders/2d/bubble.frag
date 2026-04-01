#version 330 core

in vec2  vLocalPos;
in float vAlpha;
in float vHueOffset;
in float vTime;

out vec4 fragColor;

// Free Mousse palette — blues, dusty blue, cream glints
// Hue range stays 200-270 (navy → blue → lavender)
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

    // Thin-film iridescence — hue locked to FM blue family (0.55-0.75)
    float angle = atan(vLocalPos.y, vLocalPos.x);
    float hue   = vHueOffset
                + vTime * 0.06
                + angle * 0.12
                + dist  * 0.18;

    // Keep hue in FM range: 0.55 (navy) to 0.75 (lavender)
    float h1 = 0.55 + fract(hue)       * 0.20;
    float h2 = 0.55 + fract(hue + 0.33)* 0.20;
    float h3 = 0.55 + fract(hue + 0.66)* 0.20;

    vec3 band1 = hsv2rgb(vec3(h1, 0.75, 0.90));
    vec3 band2 = hsv2rgb(vec3(h2, 0.65, 0.95));
    vec3 band3 = hsv2rgb(vec3(h3, 0.55, 1.00));

    float t1 = 0.5 + 0.5 * sin(dist * 6.28 + vTime * 1.4);
    float t2 = 0.5 + 0.5 * sin(dist * 4.71 - vTime * 1.1 + 1.0);
    vec3 iridescent = mix(mix(band1, band2, t1), band3, t2 * 0.5);

    // Rim — slightly opaque edge
    float rim   = smoothstep(0.55, 1.00, dist);
    float inner = 1.0 - smoothstep(0.0,  0.65, dist);

    // Cream glint top-left (matches logo off-white)
    vec2  gp    = vLocalPos - vec2(-0.38, 0.42);
    float glint = smoothstep(0.34, 0.0, length(gp));

    vec3  col   = iridescent;
    float alpha = vAlpha * (rim * 0.55 + inner * 0.07) + glint * 0.78;
    alpha      *= smoothstep(1.0, 0.96, dist);

    // Cream tint on glint
    col = mix(col, vec3(0.94, 0.93, 0.88), glint * 0.6);

    fragColor = vec4(col, alpha);
}
