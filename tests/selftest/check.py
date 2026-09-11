"""单 pass uniform 编码与方向"""
from utils import export, pixel


def run(exe, out):
    export(exe, ["tests/selftest/selftest.glsl", "--width", "256", "--height", "256",
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
