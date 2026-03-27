void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec4 prev = texelFetch(iChannel0, ivec2(fragCoord), 0);
    fragColor = vec4(iMouse.xy, prev.xy);
}