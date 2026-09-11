"""自引用 Buffer 的 mipmap"""
from utils import export, pixel


def run(exe, out):
    """第 5 级采样应返回 Buffer 颜色，而不是黑色（纹理不完整）"""
    export(exe, ["tests/mipmap/config.json", "--images", "0:1:1"], out)
    r, g, b = pixel(out / "00000.png", 0.5, 0.5)
    ok = abs(r - 128) <= 1 and abs(g - 64) <= 1 and abs(b - 32) <= 1
    if not ok:
        print(f"    采样得到 {(r, g, b)}，期望约 (128, 64, 32)")
    return ok
