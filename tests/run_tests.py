#!/usr/bin/env python3
"""跑 tests/ 下的自检用例，核对导出结果。

每个用例是一个最小 shader 场景，把要验证的量编码进像素值，
再用 PIL 读回来和 shader 里的预期对比。

用法：
    python tests/run_tests.py [--exe build/ShadertoyEmulator.exe] [--keep]

依赖：Pillow、numpy
"""
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np
from PIL import Image

# Windows 上 Python 默认按 cp1252 编码标准输出，打印中文用例名会抛 UnicodeEncodeError，
# 把真正要报的那个异常盖掉
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent


def pixel(png, xr, yr):
    """取相对坐标 (xr, yr) 处的像素，左上角为原点"""
    im = Image.open(png).convert("RGB")
    w, h = im.size
    return im.getpixel((min(int(w * xr), w - 1), min(int(h * yr), h - 1)))


def export(exe, args, outdir, with_output_dir=True):
    """跑一次导出。视频模式自带输出路径，不接受 --output-dir"""
    cmd = [str(exe), *args]
    if with_output_dir:
        cmd += ["--output-dir", str(outdir)]
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        # 带上退出码：崩溃（0xC0000005 之类）和程序主动返回非零，排查方向完全不同
        code = result.returncode & 0xFFFFFFFF
        raise RuntimeError(
            f"退出码 {result.returncode} (0x{code:08X})\n{' '.join(cmd)}\n{result.stdout}\n{result.stderr}"
        )


def case_selftest(exe, out):
    """单 pass：iFrame / iTime / iDate 编码 + 图像方向"""
    export(exe, ["tests/selftest.glsl", "--width", "256", "--height", "256",
                 "--images", "0:5:2"], out)
    ok = True
    for n in (0, 2, 4):
        png = out / f"{n:05d}.png"
        frame_px = pixel(png, 0.05, 0.05)[0]   # 左上角：iFrame
        time_px = pixel(png, 0.95, 0.05)[0]    # 右上角：iTime
        date_px = pixel(png, 0.05, 0.95)[0]    # 左下角：iDate
        exp_frame, exp_time = n % 256, round((n / 60.0) % 1.0 * 255)
        if not (abs(frame_px - exp_frame) <= 1 and abs(time_px - exp_time) <= 1 and date_px <= 1):
            ok = False
            print(f"    帧 {n}: iFrame={frame_px}(期望{exp_frame}) "
                  f"iTime={time_px}(期望{exp_time}) iDate={date_px}(期望≈0)")
    # shader 里 col = vec3(uv)，PNG 左上角应偏绿、右下角偏红
    tl = pixel(out / "00000.png", 0.25, 0.25)
    br = pixel(out / "00000.png", 0.75, 0.75)
    if not (tl[1] > tl[0] and br[0] > br[1]):
        ok = False
        print(f"    方向异常: 左上={tl} 右下={br}")
    return ok


def case_multipass(exe, out):
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


def case_feedback(exe, out):
    """自引用 Buffer（双缓冲）：帧 N 的累加值应为 N+1"""
    export(exe, ["tests/feedback/config.json", "--images", "0:6:1"], out)
    ok = True
    for n in range(6):
        v = pixel(out / f"{n:05d}.png", 0.5, 0.5)[0]
        if v != n + 1:
            ok = False
            print(f"    帧 {n}: 值={v} 期望={n + 1}")
    return ok


def case_mipmap(exe, out):
    """自引用 Buffer 的 mipmap：第 5 级采样应返回 Buffer 颜色，而不是黑色（纹理不完整）"""
    export(exe, ["tests/mipmap/config.json", "--images", "0:1:1"], out)
    r, g, b = pixel(out / "00000.png", 0.5, 0.5)
    ok = abs(r - 128) <= 1 and abs(g - 64) <= 1 and abs(b - 32) <= 1
    if not ok:
        print(f"    采样得到 {(r, g, b)}，期望约 (128, 64, 32)")
    return ok


def case_crossref(exe, out):
    """两个 Buffer 互相引用：BufferA = 2N+1，BufferB = 2N+2"""
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


def case_ring3(exe, out):
    """三节点成环引用：环里第一个渲染的读上一帧，其余读本帧"""
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


def case_ring4(exe, out):
    """四节点成环引用：环再长也按同样规律走"""
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


def case_selfref_read(exe, out):
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


def case_sound(exe, out):
    """Sound pass：频谱。左右声道各几个已知频率，用 FFT 核对峰值位置、幅度和声道隔离"""
    wav = out / "audio.wav"
    # 音频长度是 帧数/fps，60 帧正好 1 秒。整数频率只有落在整数号 bin 上才不会漏能量，
    # 所以窗口必须正好整秒
    export(exe, ["tests/sound/config.json", "--images", "0:60:1",
                 "--dump-audio", str(wav)], out)

    with wave.open(str(wav), "rb") as w:
        if (w.getnchannels(), w.getframerate(), w.getsampwidth()) != (2, 44100, 2):
            print(f"    格式异常: {w.getnchannels()}ch {w.getframerate()}Hz "
                  f"{w.getsampwidth() * 8}bit")
            return False
        frames = w.getnframes()
        raw = w.readframes(frames)

    if frames != 44100:
        print(f"    音频 {frames} 帧，期望 44100（正好 1 秒）")
        return False

    samples = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32767.0
    left = samples[0::2]
    right = samples[1::2]

    # 每个声道：自己该有的峰，以及只有对面才有的频率——后者有值就是串道
    channels = [
        ("左", left, [(440, 0.30), (1000, 0.20), (3000, 0.10)], (700, 2000)),
        ("右", right, [(700, 0.40), (2000, 0.25)], (440, 1000, 3000)),
    ]

    ok = True
    for name, channel, peaks, others in channels:
        # 单边谱除以 N/2 之后，峰值就等于该正弦的幅度
        mag = np.abs(np.fft.rfft(channel)) / (len(channel) / 2)

        for freq, want in peaks:
            got = mag[freq]
            if abs(got - want) > want * 0.05:
                ok = False
                print(f"    {name}声道 {freq}Hz 幅度 {got:.4f}，期望 {want}")

        for freq in others:
            if mag[freq] > 0.01:
                ok = False
                print(f"    {name}声道串入了 {freq}Hz，幅度 {mag[freq]:.4f}")
    return ok


def case_sound_common(exe, out):
    """Sound 引用 common 里的函数：公共代码只能展开一次，展开两遍会撞成重复定义"""
    wav = out / "audio.wav"
    export(exe, ["tests/sound/config-common.json", "--images", "0:60:1",
                 "--dump-audio", str(wav)], out)

    if not wav.exists():
        # shader 编译不过就不会有音频，连文件都不建
        print("    没有音频输出，Sound pass 大概没编译过")
        return False

    with wave.open(str(wav), "rb") as w:
        frames = w.getnframes()
        raw = w.readframes(frames)
    if frames != 44100:
        print(f"    音频 {frames} 帧，期望 44100（正好 1 秒）")
        return False

    samples = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32767.0

    # 两个频率都出自 common.glsl 的 tone()，宏 TAU 也是那边定义的
    ok = True
    for name, channel, freq, want in (("左", samples[0::2], 440, 0.30),
                                      ("右", samples[1::2], 700, 0.40)):
        mag = np.abs(np.fft.rfft(channel)) / (len(channel) / 2)
        if abs(mag[freq] - want) > want * 0.05:
            ok = False
            print(f"    {name}声道 {freq}Hz 幅度 {mag[freq]:.4f}，期望 {want}")
    return ok


def case_preprocessor(exe, out):
    """预处理器：宏 / 参数宏 / 条件编译 / #undef / #include，内置与外部结果必须一致"""
    args = ["tests/preprocessor/config.json", "--images", "0:1:1"]
    export(exe, args, out / "external")
    export(exe, [*args, "--builtin-preprocessor"], out / "builtin")

    ext = pixel(out / "external" / "00000.png", 0.5, 0.5)
    bi = pixel(out / "builtin" / "00000.png", 0.5, 0.5)
    expected = (64, 128, 159)

    ok = True
    if ext != bi:
        ok = False
        print(f"    外部={ext} 内置={bi}，两者应一致")
    for label, got in (("外部", ext), ("内置", bi)):
        if any(abs(a - b) > 1 for a, b in zip(got, expected)):
            ok = False
            print(f"    {label} {got}，期望 {expected}")
    return ok


def case_video(exe, out):
    """视频导出：帧数、时长、音轨，以及音画是否同步（用 ffprobe 读回来核对）"""
    fps, seconds = 30, 2
    videopath = out / "clip.mp4"
    export(exe, ["tests/sound/config.json", "--video", str(videopath),
                 "--duration", str(seconds), "--fps", str(fps),
                 "--width", "128", "--height", "128"],
           out, with_output_dir=False)

    if not videopath.exists() or videopath.stat().st_size == 0:
        print(f"    没生成视频文件: {videopath}")
        return False

    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries",
         "stream=codec_type,duration,nb_frames:format=duration", "-of", "json", str(videopath)],
        capture_output=True, text=True)
    if probe.returncode != 0:
        print(f"    ffprobe 失败: {probe.stderr.strip()}")
        return False

    info = json.loads(probe.stdout)
    streams = info.get("streams", [])
    kinds = [s.get("codec_type") for s in streams]
    if "video" not in kinds:
        print(f"    没有视频流: {kinds}")
        return False

    video = next(s for s in streams if s.get("codec_type") == "video")
    frames = int(video.get("nb_frames", 0))
    if frames != seconds * fps:
        print(f"    帧数 {frames}，期望 {seconds * fps}")
        return False

    # 时长优先取流自己的，取不到再退回容器的
    vdur = float(video.get("duration") or info.get("format", {}).get("duration", 0))
    if abs(vdur - seconds) > 0.2:
        print(f"    视频时长 {vdur:.2f}s，期望 {seconds}s")
        return False

    if "audio" not in kinds:
        print(f"    shader 有 Sound pass 但视频里没有音轨: {kinds}")
        return False

    # 音画同步：音频也该是 2 秒，而不是按"每帧 0.5 秒"累积出来的 60 秒
    audio = next(s for s in streams if s.get("codec_type") == "audio")
    adur = float(audio.get("duration") or info.get("format", {}).get("duration", 0))
    if abs(adur - seconds) > 0.2:
        print(f"    音频时长 {adur:.2f}s，期望 {seconds}s（音画不同步）")
        return False
    return True


def case_images(exe, out):
    """图片序列导出：帧范围/步长/文件名，以及中间帧确实被渲染过"""
    # start 非 0、step 非 1：程序要把 0..11 每一帧都渲染一遍（buffer 状态得连续），
    # 但只存 5/8/11。若它图省事只渲染被保存的那几帧，iFrame 就会是 0/1/2
    export(exe, ["tests/selftest.glsl", "--width", "128", "--height", "128",
                 "--images", "5:12:3"], out)

    expected = [5, 8, 11]
    got = sorted(p.name for p in out.glob("*.png"))
    want = [f"{n:05d}.png" for n in expected]
    if got != want:
        print(f"    导出文件 {got}，期望 {want}")
        return False

    ok = True
    for n in expected:
        png = out / f"{n:05d}.png"
        frame_px = pixel(png, 0.05, 0.05)[0]   # 左上角：iFrame
        time_px = pixel(png, 0.95, 0.05)[0]    # 右上角：iTime
        exp_frame, exp_time = n % 256, round((n / 60.0) % 1.0 * 255)
        if abs(frame_px - exp_frame) > 1 or abs(time_px - exp_time) > 1:
            ok = False
            print(f"    帧 {n}: iFrame={frame_px}(期望{exp_frame}) iTime={time_px}(期望{exp_time})")
    return ok


def case_passres(exe, out):
    """Buffer 独立分辨率：buffer 内看到的 iResolution 应是它自己的，而非窗口的"""
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


def case_texture(exe, out):
    """文件纹理通道：加载、采样值，以及 flipY 的两个方向"""
    PNG = [
        [(255, 0, 0),    (0, 255, 0),    (0, 0, 255),     (255, 255, 255)],
        [(128, 64, 32),  (10, 20, 30),   (200, 100, 50),  (255, 128, 0)],
        [(255, 255, 0),  (0, 255, 255),  (255, 0, 255),   (0, 0, 0)],
        [(77, 77, 77),   (1, 2, 3),      (254, 253, 252), (100, 150, 200)],
    ]
    # 4x4 格子在屏幕上的中心（相对坐标）：上方是第 0 行，下方是第 3 行
    LEFT_X, RIGHT_X = 1 / 8, 7 / 8
    TOP_Y, BOTTOM_Y = 1 / 8, 7 / 8

    ok = True
    for cfg, flipped in (("tests/texture/config.json", False),
                         ("tests/texture/config-flip.json", True)):
        sub = out / ("flip" if flipped else "plain")
        export(exe, [cfg, "--images", "0:1:1"], sub)

        # 纹理按原样上传时 PNG 的第一行落在 GL 纹理的底边，屏幕上方反而对应 PNG 的最后
        # 一行；flipY 打开后才所见即所得。两种都是既有行为，都得锁住
        row_top, row_bottom = (0, 3) if flipped else (3, 0)
        checks = [
            ("左上", LEFT_X, TOP_Y, PNG[row_top][0]),
            ("右上", RIGHT_X, TOP_Y, PNG[row_top][3]),
            ("左下", LEFT_X, BOTTOM_Y, PNG[row_bottom][0]),
        ]

        for name, xr, yr, want in checks:
            got = pixel(sub / "00000.png", xr, yr)
            if any(abs(a - b) > 1 for a, b in zip(got, want)):
                ok = False
                print(f"    {'flipY' if flipped else '原样'} {name}: {got}，期望 {want}")
    return ok


CASES = [
    ("selftest", "单 pass uniform 编码与方向", case_selftest),
    ("multipass", "多 pass 帧同步", case_multipass),
    ("images", "图片序列导出（帧范围/步长/中间帧）", case_images),
    ("passres", "Buffer 独立分辨率", case_passres),
    ("texture", "文件纹理通道（加载/采样/flipY）", case_texture),
    ("feedback", "自引用 Buffer 累加", case_feedback),
    ("mipmap", "自引用 Buffer 的 mipmap", case_mipmap),
    ("crossref", "双 Buffer 互相引用", case_crossref),
    ("ring3", "三节点成环引用", case_ring3),
    ("ring4", "四节点成环引用", case_ring4),
    ("selfref_read", "自引用 Buffer 被读取的时序", case_selfref_read),
    ("sound", "Sound 频谱（频率/幅度/声道隔离）", case_sound),
    ("sound-common", "Sound + common（函数定义不重复展开）", case_sound_common),
    ("preprocessor", "预处理器（内置 vs 外部对照）", case_preprocessor),
    ("video", "视频导出（帧数/时长/音轨同步）", case_video),
]


def main():
    parser = argparse.ArgumentParser(description="跑 tests/ 下的自检用例")
    parser.add_argument("--exe", default="build/ShadertoyEmulator.exe", help="模拟器可执行文件")
    parser.add_argument("--keep", action="store_true", help="保留导出的 PNG 便于排查")
    args = parser.parse_args()

    exe = (ROOT / args.exe).resolve()
    if not exe.exists():
        print(f"找不到可执行文件：{exe}", file=sys.stderr)
        return 1

    workdir = Path(tempfile.mkdtemp(prefix="shadertoy_tests_"))
    failures = 0
    try:
        for slug, title, fn in CASES:
            try:
                ok = fn(exe, workdir / slug)
            except Exception as exc:
                ok = False
                print(f"    异常: {exc}")
            print(f"[{'PASS' if ok else 'FAIL'}] {title}")
            failures += 0 if ok else 1
    finally:
        if args.keep:
            print(f"\n导出结果保留在 {workdir}")
        else:
            shutil.rmtree(workdir, ignore_errors=True)

    print(f"\n{len(CASES) - failures}/{len(CASES)} 通过")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
