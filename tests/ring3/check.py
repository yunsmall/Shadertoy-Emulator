"""三节点成环引用"""
from utils import export, pixel


def run(exe, out):
    """环里第一个渲染的读上一帧，其余读本帧"""
    export(exe, ["tests/ring3/config.json", "--images", "0:4:1"], out)
    ok = True
    for n in range(4):
        png = out / f"{n:05d}.png"
        a = pixel(png, 1 / 6, 0.5)[0]
        b = pixel(png, 0.5, 0.5)[0]
        c = pixel(png, 5 / 6, 0.5)[0]
        if (a, b, c) != (3 * n + 1, 3 * n + 2, 3 * n + 3):
            ok = False
            print(f"    帧 {n}: A={a} B={b} C={c}（期望 {3 * n + 1}/{3 * n + 2}/{3 * n + 3}）")
    return ok
