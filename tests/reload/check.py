"""热重载：点重载后新 shader 生效；编译不过时保留原有 shader、程序照常跑完"""
import subprocess
import time

from utils import ROOT, export, pixel

SHADER = ROOT / "tests" / "reload" / "image.glsl"
# 每帧都得渲染，否则重载点所在的帧根本不会被渲染（图片模式默认跳中间帧）
ARGS = ["tests/reload/config.json", "--images", "0:4001:4000", "--render-all-frames"]
RELOAD_FRAME = 2000

# 换上去的 0.75, 0.25, 0.5 落到 8 位是这个值。旧颜色不写死——先跑一趟原样的读出来
# 当基准，浮点转 8 位怎么舍入不值得在这个用例里赌
NEW_COLOR = (191, 64, 127)
BROKEN = ("void mainImage(out vec4 fragColor, in vec2 fragCoord) {\n"
          "    fragColor = vec4(1.0\n}\n")


def _run_replacing_shader(exe, outdir, new_text):
    """跑一趟，中途把 shader 换成 new_text，返回 (退出码, 全部输出)。

    等第 0 帧落盘再动手：那时 exe 已经读完配置和 shader，改的就是重载会读到的那份。
    换早了它启动就编译不过，换晚了重载那一下已经过去了
    """
    proc = subprocess.Popen(
        [str(exe), *ARGS, "--reload-at-frame", str(RELOAD_FRAME), "--output-dir", str(outdir)],
        cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    first = outdir / "00000.png"
    for _ in range(2000):
        if first.exists():
            break
        time.sleep(0.01)
    SHADER.write_text(new_text, encoding="utf-8")

    out, err = proc.communicate()
    return proc.returncode, out + err


def _last_frame_color(path, want, label, log, ok):
    if not path.exists():
        print(f"    {label}：没有导出最后一帧")
        return False
    got = pixel(path, 0.5, 0.5)
    if got != want:
        print(f"    {label}：最后一帧是 {got}，期望 {want}\n{log}")
        return False
    return ok


def run(exe, out):
    ok = True
    original = SHADER.read_text(encoding="utf-8")

    try:
        # 先原样跑一趟，把"没重载时最后一帧是什么颜色"记下来当基准
        export(exe, [*ARGS], out / "base")
        old_color = pixel(out / "base" / "04000.png", 0.5, 0.5)

        # 换成另一种颜色：重载成功的话，最后一帧该由新 shader 渲染
        rc, log = _run_replacing_shader(
            exe, out / "ok", original.replace("0.25, 0.5, 0.75", "0.75, 0.25, 0.5"))
        if rc != 0:
            ok = False
            print(f"    重载成功那趟退出码 {rc}：\n{log}")
        if "Reloaded config" not in log:
            ok = False
            print(f"    没有重载成功的日志：\n{log}")
        ok = _last_frame_color(out / "ok" / "04000.png", NEW_COLOR, "重载成功后", log, ok)

        # 先还原再跑第二趟：不还原的话它启动时读到的还是上一趟留下的新颜色，
        # "保留原有 shader"保的就成了那份新的
        SHADER.write_text(original, encoding="utf-8")

        # 换成语法错的：重载该失败，画面继续由原来那个 shader 渲染，程序正常跑完
        rc, log = _run_replacing_shader(exe, out / "broken", BROKEN)
        if rc != 0:
            ok = False
            print(f"    重载失败那趟退出码 {rc}，不该中断：\n{log}")
        if "Reload failed" not in log:
            ok = False
            print(f"    没有重载失败的日志：\n{log}")
        ok = _last_frame_color(out / "broken" / "04000.png", old_color, "重载失败后", log, ok)
    finally:
        SHADER.write_text(original, encoding="utf-8")

    return ok
