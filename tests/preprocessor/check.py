"""预处理器（内置 vs 外部对照）"""
from utils import export, pixel


def run(exe, out):
    """宏 / 参数宏 / 条件编译 / #undef / #include，内置与外部结果必须一致"""
    args = ["tests/preprocessor/config.json", "--images", "0:1:1"]
    export(exe, args, out / "external")
    export(exe, [*args, "--builtin-preprocessor"], out / "builtin")

    ext = pixel(out / "external" / "00000.png", 0.5, 0.5)
    bi = pixel(out / "builtin" / "00000.png", 0.5, 0.5)
    expected = (64, 128, 159)

    ok = True
    if ext != bi:
        ok = False
        print(f"    外部={ext} 内置={bi}，两者应一致")
    for label, got in (("外部", ext), ("内置", bi)):
        if any(abs(a - b) > 1 for a, b in zip(got, expected)):
            ok = False
            print(f"    {label} {got}，期望 {expected}")
    return ok
