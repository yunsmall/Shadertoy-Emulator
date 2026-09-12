"""图片序列导出（帧范围/多区间/中间帧）"""
from utils import export, pixel


def run(exe, out):
    ok = True
    # 复用 selftest 的场景：它把 iFrame/iTime 编码进了像素
    # 这里盯的是"--render-all-frames 一帧不落地渲染"那条路：start 非 0、step 非 1 时
    # 要把 0..11 全跑一遍，只存 5/8/11。若它图省事只渲染被保存的那几帧，iFrame 就会是
    # 0/1/2。默认那条跳帧的路由 skipintermediate 用例盯着
    export(exe, ["tests/selftest/selftest.glsl", "--width", "128", "--height", "128",
                 "--images", "5:12:3", "--render-all-frames"], out)

    expected = [5, 8, 11]
    got = sorted(p.name for p in out.glob("*.png"))
    want = [f"{n:05d}.png" for n in expected]
    if got != want:
        ok = False
        print(f"    导出文件 {got}，期望 {want}")

    for n in expected:
        png = out / f"{n:05d}.png"
        frame_px = pixel(png, 0.05, 0.05)[0]   # 左上角：iFrame
        time_px = pixel(png, 0.95, 0.05)[0]    # 右上角：iTime
        exp_frame, exp_time = n % 256, round((n / 60.0) % 1.0 * 255)
        if abs(frame_px - exp_frame) > 1 or abs(time_px - exp_time) > 1:
            ok = False
            print(f"    帧 {n}: iFrame={frame_px}(期望{exp_frame}) iTime={time_px}(期望{exp_time})")

    # 帧可以来自多个区间：--images 能给多次，也能逗号分隔，重叠的部分只挑一次
    cases = [
        ("multi", ["--images", "0:3:1", "--images", "10:12:1"], [0, 1, 2, 10, 11]),
        ("comma", ["--images", "0:3:1,10:12:1"], [0, 1, 2, 10, 11]),
        ("union", ["--images", "0:6:2", "--images", "4:10:2"], [0, 2, 4, 6, 8]),
    ]
    for slug, args, frames in cases:
        sub = out / slug
        r = export(exe, ["tests/selftest/selftest.glsl", "--width", "128", "--height", "128",
                         *args, "--render-all-frames"], sub)
        # 存了几张只能看报出来的数：重叠的帧存两遍也只是把同名文件覆盖一遍，
        # 从文件名上看不出来
        if f"{len(frames)} frames" not in r.stdout:
            ok = False
            print(f"    {slug}: 帧数不对，应为 {len(frames)} -> "
                  f"{r.stdout.splitlines()[-1] if r.stdout else '(无输出)'}")
        # 帧号按原样保留：第 10 帧就是 00010.png，不能因为它在集合里排第 4 就写成 00004
        got = sorted(p.name for p in sub.glob("*.png"))
        want = [f"{n:05d}.png" for n in frames]
        if got != want:
            ok = False
            print(f"    {slug}: 导出 {got}，期望 {want}")
        for n in frames:
            frame_px = pixel(sub / f"{n:05d}.png", 0.05, 0.05)[0]
            if abs(frame_px - n % 256) > 1:
                ok = False
                print(f"    {slug} 帧 {n}: iFrame 读回 {frame_px}，期望 {n}")

    # 强制跳帧那条路是按目标帧跳着走的，重叠区间里的同一帧不能处理两次——存同名文件
    # 只是覆盖一遍，从文件上看不出来，但 Exported 的张数会多算
    r = export(exe, ["tests/selftest/selftest.glsl", "--width", "128", "--height", "128",
                     "--images", "0:6:2", "--images", "4:10:2", "--force-skip-intermediate"],
               out / "union_force")
    if "5 frames" not in r.stdout:
        ok = False
        print(f"    union(force): 去重后应存 5 张 -> "
              f"{r.stdout.splitlines()[-1] if r.stdout else '(无输出)'}")

    return ok
