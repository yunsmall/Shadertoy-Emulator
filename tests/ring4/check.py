"""四节点成环引用"""
from utils import export, pixel


def run(exe, out):
    """环再长也按同样规律走"""
    export(exe, ["tests/ring4/config.json", "--images", "0:4:1"], out)
    ok = True
    for n in range(4):
        png = out / f"{n:05d}.png"
        got = [pixel(png, i * 0.25 + 0.125, 0.5)[0] for i in range(4)]
        exp = [4 * n + 1, 4 * n + 2, 4 * n + 3, 4 * n + 4]
        if got != exp:
            ok = False
            print(f"    帧 {n}: {got}（期望 {exp}）")
    return ok
