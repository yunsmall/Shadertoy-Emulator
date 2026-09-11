"""Buffer 的 alpha 通道（数据）/ Image 的 alpha（钉成 1）"""
from utils import export, pixel_rgba


def run(exe, out):
    """两个通道的包装不能合并成一套：Buffer 抹掉 alpha 会让拿它存状态的 shader
    （流体、粒子、渐进式路径追踪）帧间数据全丢；Image 不盖 alpha 又会让窗口和
    导出的 PNG 变全透明。
    """
    export(exe, ["tests/alphachan/config.json", "--images", "0:5:1"], out)
    ok = True
    for n in range(5):
        r, _, _, a = pixel_rgba(out / f"{n:05d}.png", 0.5, 0.5)
        # BufferA 每帧把 alpha 加 1，第 n 帧读到 n+1，shader 除以 16 显示成灰度；
        # alpha 被抹成 1 的话每帧都是同一个值
        want = round((n + 1) / 16.0 * 255)
        if abs(r - want) > 1:
            ok = False
            print(f"    帧 {n}: BufferA 的 alpha 读回 {r}，期望 {want}"
                  f"（被抹成 1 的话恒为 {round(1 / 16.0 * 255)}）")
        if a != 255:
            ok = False
            print(f"    帧 {n}: Image 输出的 alpha={a}，期望 255")
    return ok
