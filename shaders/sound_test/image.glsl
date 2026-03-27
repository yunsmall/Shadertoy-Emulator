// Simple visualization for sound test

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;

    // Simple animated pattern
    float t = iTime * 0.5;
    vec3 col = vec3(
        sin(uv.x * 3.14159 + t) * 0.5 + 0.5,
        sin(uv.y * 3.14159 + t * 1.3) * 0.5 + 0.5,
        sin((uv.x + uv.y) * 3.14159 + t * 0.7) * 0.5 + 0.5
    );

    fragColor = vec4(col, 1.0);
}
