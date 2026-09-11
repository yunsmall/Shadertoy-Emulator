"""图片序列导出（帧范围/步长/中间帧）"""
from utils import export, pixel


def run(exe, out):
    # 复用 selftest 的场景：它把 iFrame/iTime 编码进了像素
    # start 非 0、step 非 1：程序要把 0..11 每一帧都渲染一遍（buffer 状态得连续），
    # 但只存 5/8/11。若它图省事只渲染被保存的那几帧，iFrame 就会是 0/1/2
    export(exe, ["tests/selftest/selftest.glsl", "--width", "128", "--height", "128",
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
