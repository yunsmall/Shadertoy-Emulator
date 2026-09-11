"""--skip-intermediate：只渲染选中的帧"""
from utils import export, pixel


def run(exe, out):
    # 自引用 Buffer 的累加值就是"渲染了几帧"的探针：导出 5/8/11 三帧，全渲染时
    # Buffer 已经走过 12 帧，跳帧时只走了 3 帧，两种结果必须对得上各自的预期
    args = ["tests/skipintermediate/config.json", "--images", "5:12:3"]
    export(exe, args, out / "full")
    export(exe, [*args, "--skip-intermediate"], out / "skip")

    ok = True
    for slug, want in (("full", (6, 9, 12)), ("skip", (1, 2, 3))):
        got = tuple(pixel(out / slug / f"{n:05d}.png", 0.5, 0.5)[0] for n in (5, 8, 11))
        if got != want:
            ok = False
            print(f"    {slug}: BufferA 的累加值 {got}，期望 {want}")

    # 中间帧不渲染不等于帧号缩水：留下来的帧，iFrame 还得是它自己的帧号
    export(exe, ["tests/selftest/selftest.glsl", "--width", "128", "--height", "128",
                 "--images", "5:12:3", "--skip-intermediate"], out / "frame")
    for n in (5, 8, 11):
        frame_px = pixel(out / "frame" / f"{n:05d}.png", 0.05, 0.05)[0]
        if abs(frame_px - n % 256) > 1:
            ok = False
            print(f"    帧 {n}: iFrame 读回 {frame_px}，期望 {n}")
    return ok
