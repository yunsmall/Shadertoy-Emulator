"""自引用 Buffer 累加"""
from utils import export, pixel


def run(exe, out):
    """自引用 Buffer（双缓冲）：帧 N 的累加值应为 N+1"""
    export(exe, ["tests/feedback/config.json", "--images", "0:6:1"], out)
    ok = True
    for n in range(6):
        v = pixel(out / f"{n:05d}.png", 0.5, 0.5)[0]
        if v != n + 1:
            ok = False
            print(f"    帧 {n}: 值={v} 期望={n + 1}")
    return ok
