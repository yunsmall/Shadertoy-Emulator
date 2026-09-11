"""双 Buffer 互相引用"""
from utils import export, pixel


def run(exe, out):
    """BufferA = 2N+1，BufferB = 2N+2"""
    export(exe, ["tests/crossref/config.json", "--images", "0:6:1"], out)
    ok = True
    for n in range(6):
        png = out / f"{n:05d}.png"
        a = pixel(png, 0.25, 0.5)[0]
        b = pixel(png, 0.75, 0.5)[0]
        if a != 2 * n + 1 or b != 2 * n + 2:
            ok = False
            print(f"    帧 {n}: BufferA={a}(期望{2 * n + 1}) BufferB={b}(期望{2 * n + 2})")
    return ok
