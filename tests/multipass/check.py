"""多 pass 帧同步"""
from utils import export, pixel


def run(exe, out):
    """BufferA 与 Image 的 iFrame 必须一致（差一帧是历史 bug）"""
    export(exe, ["tests/multipass/config.json", "--images", "0:5:1"], out)
    ok = True
    for n in range(5):
        png = out / f"{n:05d}.png"
        left = pixel(png, 0.25, 0.5)[0]    # BufferA 记录的 iFrame
        right = pixel(png, 0.75, 0.5)[0]   # Image 自己看到的 iFrame
        if left != n or right != n:
            ok = False
            print(f"    帧 {n}: BufferA={left} Image={right}（都应为 {n}）")
    return ok
