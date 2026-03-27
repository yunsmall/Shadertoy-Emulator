void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    // hold mouse button + move to move around scene
    fragColor = vec4(texelFetch(iChannel0, ivec2(fragCoord), 0).xyz,1.0);
}