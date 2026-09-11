// 把 4x4 的 pattern.png 放大到整屏，每格 16x16 像素。
// 用 texelFetch 直接取整格而不是 texture()：哪怕是 nearest 过滤，采样点落在
// 格子边界附近也会掺进邻格的颜色，断言就没法用精确值了
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    ivec2 texel = ivec2(floor(fragCoord / (iResolution.xy / 4.0)));
    fragColor = texelFetch(iChannel0, texel, 0);
}
