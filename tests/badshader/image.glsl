// 故意少一个右括号：这个 shader 必然编译不过，用来验证程序会就此打住，
// 而不是跳过去接着渲染（那样导出的就是一堆黑图）
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0
}
