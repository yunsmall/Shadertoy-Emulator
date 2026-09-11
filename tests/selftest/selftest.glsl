// 自检用：把各 uniform 编码进画面，导出后用脚本核对像素值
//   底色        R = uv.x, G = uv.y    —— 验证坐标系方向和上下翻转
//   左上角方块  灰度 = iFrame & 255    —— 验证帧号
//   右上角方块  灰度 = fract(iTime)    —— 验证虚拟时间
//   左下角方块  灰度 = iDate.w / 86400 —— 离屏下应接近 0（固定纪元），而非墙钟的半天左右
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    vec3 col = vec3(uv, 0.0);

    if (uv.x < 0.12 && uv.y > 0.88) {
        col = vec3(mod(float(iFrame), 256.0) / 255.0);
    } else if (uv.x > 0.88 && uv.y > 0.88) {
        col = vec3(fract(iTime));
    } else if (uv.x < 0.12 && uv.y < 0.12) {
        col = vec3(iDate.w / 86400.0);
    }

    fragColor = vec4(col, 1.0);
}
