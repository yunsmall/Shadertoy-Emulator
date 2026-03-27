// Shadertoy Test Shader
// This shader tests all basic uniforms

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord / iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0.0, 2.0, 4.0));

    // Mouse interaction - draw a circle where mouse is
    if (iMouse.z > 0.0 || iMouse.w > 0.0) {
        vec2 mousePos = iMouse.xy / iResolution.xy;
        float d = length(uv - mousePos);
        col = mix(col, vec3(1.0), smoothstep(0.05, 0.02, d));
    }

    // Frame counter visualization (subtle red tint based on frame)
    col.r += 0.1 * sin(float(iFrame) * 0.1);

    // Output to screen
    fragColor = vec4(col, 1.0);
}
