"""自引用 Buffer 被读取的时序"""
from utils import export, pixel


def run(exe, out):
    """自引用的 Buffer 被别的 pass 读到的是本帧的值，不是上一帧"""
    export(exe, ["tests/selfref_read/config.json", "--images", "0:4:1"], out)
    ok = True
    for n in range(4):
        png = out / f"{n:05d}.png"
        a = pixel(png, 0.25, 0.5)[0]
        b = pixel(png, 0.75, 0.5)[0]
        if a != n + 1 or b != 10 * (n + 1):
            ok = False
            print(f"    帧 {n}: A={a}(期望{n + 1}) B={b}(期望{10 * (n + 1)})")
    return ok
