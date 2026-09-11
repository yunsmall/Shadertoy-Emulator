"""文件纹理通道（加载/采样/flipY）"""
from utils import export, pixel


def run(exe, out):
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
