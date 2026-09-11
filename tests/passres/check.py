"""Buffer 独立分辨率"""
from utils import export, pixel


def run(exe, out):
    """buffer 内看到的 iResolution 应是它自己的，而非窗口的"""
    export(exe, ["tests/passres/config.json", "--images", "0:1:1"], out)
    png = out / "00000.png"
    ok = True
    # 32x32 编码成 0.25 → 读回 64；若错拿窗口的 64x64 则是 0.5 → 读回 128
    r, g, _ = pixel(png, 0.5, 0.25)
    if abs(r - 64) > 1 or abs(g - 64) > 1:
        ok = False
        print(f"    BufferA 内 iResolution 读回 {(r, g)}，期望 (64, 64) 即 32x32")
    r, g, _ = pixel(png, 0.5, 0.75)
    if abs(r - 64) > 1 or abs(g - 64) > 1:
        ok = False
        print(f"    iChannelResolution[0] 读回 {(r, g)}，期望 (64, 64) 即 32x32")
    return ok
