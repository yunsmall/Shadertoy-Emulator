"""未初始化变量补零：本地不是 WebGL，桌面驱动给的是寄存器残值而不是 0"""
from utils import export, pixel

# tricky.glsl 里每个检查占一个像素（x = 检查编号），按表对号入座
TRICKY = [
    "float a;",
    "五项挤一行 float b, c, d, e, f;",
    "跨行多声明 float g, h, i2;",
    "混初值 float x1 = 1.5, x2;",
    "vec2/vec3/vec4",
    "ivec2 / uvec2 / bvec2",
    "mat2 / mat3x4",
    "highp float 精度限定符",
    "全局变量 float gFloat; vec3 gVec; ivec2 gIVec;",
    "bool / int / uint",
    "初值带函数调用 max(1.0, 2.0) 后面的声明",
    "初值带括号 (1.0 + 2.0) * 3.0 后面的声明",
    "数组声明 float withArray[2]; 后面的变量",
    "带初值数组 float zz[2] = float[2](1.0, 2.0); 后面的变量",
    "三目初值后面的声明",
    "科学计数法初值后面的声明",
    "名字带数字和下划线 float v1, v2_;",
    "if 块内声明",
    "for 块内声明",
    "函数内局部变量",
    "const / uniform 没被误加初值",
    "一行两条语句 float q1; float q2;",
    "紧贴注释的声明",
    "struct 变量跳过（显式赋的值还在）",
]


def run(exe, out):
    ok = True

    # autoinit.glsl 把 12 项检查汇总进红色通道，全对是 255。哪一项没通过都会把
    # 红色压低，反过来念就是"过了几项"。两条预处理路径都要过这一关
    for slug, extra in [("default", []), ("builtin", ["--builtin-preprocessor"])]:
        sub = out / slug
        export(exe, ["tests/zeroinit/autoinit.glsl", "--width", "64", "--height", "64",
                     "--images", "0:1:1", *extra], sub)
        value = pixel(sub / "00000.png", 0.5, 0.5)[0]
        if value != 255:
            ok = False
            print(f"    {slug}: 12 项检查只过了 {round(value / 255.0 * 12)} 项"
                  f"（红色 {value}，应为 255）")

    # 刁钻场景逐项看，哪一项红不了就直接报出来
    export(exe, ["tests/zeroinit/tricky.glsl", "--width", "32", "--height", "8",
                 "--images", "0:1:1"], out / "tricky")
    png = out / "tricky" / "00000.png"
    for i, desc in enumerate(TRICKY):
        value = pixel(png, (i + 0.5) / 32.0, 0.5)[0]
        if value != 255:
            ok = False
            print(f"    tricky[{i}] {desc}: 红 {value}，应为 255")

    # CRLF 换行 + 中文注释：Windows 风格的行尾和多字节注释都不能让扫描器错位
    # （注释那些字节曾经会被某些 locale 当成字母）
    crlf = (
        "// 中文注释：下面是未初始化的变量，应当被补成 0\r\n"
        "void mainImage(out vec4 fragColor, in vec2 fragCoord) {\r\n"
        "    float cn;\r\n"
        "    vec3 cnv;\r\n"
        "    /* 块注释里的假声明：float ghost; */\r\n"
        "    fragColor = vec4((cn == 0.0 && cnv.x == 0.0) ? 1.0 : 0.0, 0.0, 0.0, 1.0);\r\n"
        "}\r\n"
    )
    crlf_dir = out / "crlf"
    crlf_dir.mkdir(parents=True, exist_ok=True)
    (crlf_dir / "crlf.glsl").write_bytes(crlf.encode("utf-8"))
    export(exe, [str(crlf_dir / "crlf.glsl"), "--width", "8", "--height", "8",
                 "--images", "0:1:1"], crlf_dir / "img")
    value = pixel(crlf_dir / "img" / "00000.png", 0.5, 0.5)[0]
    if value != 255:
        ok = False
        print(f"    CRLF + 中文注释：红 {value}，应为 255")

    return ok
