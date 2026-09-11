"""--skip-intermediate：只跳过没被依赖的中间帧（force 版一律跳）"""
import subprocess

from utils import ROOT, export, images_equal, pixel


def run(exe, out):
    ok = True

    # 最硬的一条：跳帧和全渲染必须逐像素一致。依赖分析只要漏掉一个该跑的通道，
    # 值就会分叉。三个场景各盯一头：自引用本身、传播到无状态的上游、绕一圈的环
    for name, cfg in (("selfref", "tests/skipintermediate/config.json"),
                      ("chain", "tests/skipintermediate/chain.json"),
                      ("ring", "tests/ring3/config.json")):
        args = [cfg, "--images", "0:12:3"]
        export(exe, args, out / name / "full")
        r = export(exe, [*args, "--skip-intermediate"], out / name / "skip")
        bad = images_equal(out / name / "full", out / name / "skip")
        if bad:
            ok = False
            print(f"    {name}: 跳帧和全渲染对不上（{bad}），依赖分析漏了通道")
        # 跳了哪些通道外面看不出来，得说一声。这条也顺带盯着分析结果本身：
        # 三个场景的 BufferA 都在每帧跑的名单里
        if "Skipping frames in between" not in r.stdout or "BufferA" not in r.stdout:
            ok = False
            print(f"    {name}: 没说明跳帧模式下哪些通道照常渲染")

    # 强制跳帧不做分析，中间帧一帧不跑，自引用 Buffer 的累加值必然和全渲染不同。
    # 量出来正好是 1/2/3 就说明它确实只渲染了存盘的那 3 帧
    force_args = ["tests/skipintermediate/config.json", "--images", "5:12:3"]
    r = export(exe, [*force_args, "--force-skip-intermediate"], out / "force")
    got = tuple(pixel(out / "force" / f"{n:05d}.png", 0.5, 0.5)[0] for n in (5, 8, 11))
    if got != (1, 2, 3):
        ok = False
        print(f"    force: BufferA 的累加值 {got}，期望 (1, 2, 3)——中间帧确实没跑")

    # 结果算错了得说一声，不能闷着
    if "feedback state is wrong" not in r.stderr:
        ok = False
        print(f"    force: 自引用 Buffer 没给出警告")

    # 两种模式都得保留原帧号：中间帧不渲染不等于帧号缩水成 0/1/2
    for slug, flag in (("analyze", "--skip-intermediate"),
                       ("force", "--force-skip-intermediate")):
        export(exe, ["tests/selftest/selftest.glsl", "--width", "128", "--height", "128",
                     "--images", "5:12:3", flag], out / f"frame_{slug}")
        for n in (5, 8, 11):
            frame_px = pixel(out / f"frame_{slug}" / f"{n:05d}.png", 0.05, 0.05)[0]
            if abs(frame_px - n % 256) > 1:
                ok = False
                print(f"    {slug} 帧 {n}: iFrame 读回 {frame_px}，期望 {n}")

    # 两个参数是互斥的，同时给得报错而不是挑一个默默用
    r = subprocess.run(
        [str(exe), "tests/skipintermediate/config.json", "--images", "0:1:1",
         "--output-dir", str(out / "both"),
         "--skip-intermediate", "--force-skip-intermediate"],
        cwd=ROOT, capture_output=True, text=True)
    if r.returncode == 0:
        ok = False
        print("    同时给 --skip-intermediate 和 --force-skip-intermediate 应该报错退出")

    return ok
