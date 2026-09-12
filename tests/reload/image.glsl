// 固定颜色。热重载测试靠它分辨最后一帧是新 shader 渲染的还是旧 shader 留下的：
// 用例会把这个文件换成另一种颜色，再换成语法错的
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(0.25, 0.5, 0.75, 1.0);
}
