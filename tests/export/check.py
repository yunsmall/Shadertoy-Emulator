"""--export：展开 #include、路径改成 ./，导出的目录能原样再跑"""
import json
import re
import shutil
import subprocess

from utils import ROOT, export, images_equal


def _export(exe, args):
    """跑一次 --export。它不渲染，utils.export() 那套 --output-dir 用不上"""
    return subprocess.run([str(exe), *args], cwd=ROOT, capture_output=True, text=True)


def run(exe, out):
    ok = True
    packed = out / "packed"

    proc = _export(exe, ["tests/export/config.json", "--export", str(packed)])
    if proc.returncode != 0:
        print(f"    导出失败（退出码 {proc.returncode}）:\n{proc.stdout}\n{proc.stderr}")
        return False

    # 被 #include 的 helper.glsl 不该单独出现在输出目录里，它的内容已经内联了
    names = sorted(p.name for p in packed.iterdir())
    want = ["buffera.glsl", "common.glsl", "config.json", "image.glsl", "noise.png"]
    if names != want:
        ok = False
        print(f"    输出目录是 {names}，期望 {want}")

    # 展开干净了：内容进来了，指令没留下
    image = (packed / "image.glsl").read_text(encoding="utf-8")
    if "#define HELPER_BIAS 0.25" not in image:
        ok = False
        print("    helper.glsl 的内容没被展开进来")
    # 按行首匹配：注释里提一嘴 "#include" 不算，真指令才拦
    for token in (r"^\s*#\s*include", r"^\s*#\s*line"):
        if re.search(token, image, re.MULTILINE):
            ok = False
            print(f"    image.glsl 里还留着 {token}")

    # 各展各的：common 和 image 都 #include 了 shared.glsl，两边都该展开
    common = (packed / "common.glsl").read_text(encoding="utf-8")
    for label, text in (("common.glsl", common), ("image.glsl", image)):
        if "#define SHARED_GAIN 0.1" not in text:
            ok = False
            print(f"    {label} 里没展开 shared.glsl")

    # 路径字段全改成 ./*：buffer 的 source 写的是通道名不是文件，不能跟着改
    cfg = json.loads((packed / "config.json").read_text(encoding="utf-8"))
    # $schema 只管编辑器补全，导出件是独立出去的，留着只会指到一个不在那儿的地方
    if "$schema" in cfg:
        ok = False
        print(f"    导出的配置里还留着 $schema：{cfg['$schema']}")
    if cfg["common"] != "./common.glsl":
        ok = False
        print(f"    common = {cfg['common']}")
    for p in cfg["passes"]:
        if not p["shader"].startswith("./"):
            ok = False
            print(f"    shader = {p['shader']}")
    channels = cfg["passes"][1]["channels"]
    if channels["0"]["source"] != "./noise.png":
        ok = False
        print(f"    texture 的 source = {channels['0']['source']}")
    if channels["1"]["source"] != "BufferA":
        ok = False
        print(f"    buffer 的 source 被改动了：{channels['1']['source']}")

    # 导出的目录原样再跑一遍，画面得和原目录一模一样——展开和改写有没有走样，
    # 这一条比逐字段检查更能说明问题
    export(exe, ["tests/export/config.json", "--images", "0:1:1"], out / "before")
    export(exe, [str(packed / "config.json"), "--images", "0:1:1"], out / "after")
    bad = images_equal(out / "before", out / "after")
    if bad:
        ok = False
        print(f"    导出前后画面不一致：{bad}")

    # 单 glsl 输入：只出一个展开好的文件
    single = out / "single"
    proc = _export(exe, ["tests/export/image.glsl", "--export", str(single)])
    if proc.returncode != 0:
        ok = False
        print(f"    单文件导出失败：\n{proc.stderr}")
    else:
        got = sorted(p.name for p in single.iterdir())
        if got != ["image.glsl"]:
            ok = False
            print(f"    单文件输出是 {got}，期望 ['image.glsl']")

    # --clean-export：上一轮多出来的文件不该留着。放在最后做，它会重导一遍 packed
    (packed / "leftover.txt").write_text("上一轮多出来的", encoding="utf-8")
    proc = _export(exe, ["tests/export/config.json", "--export", str(packed), "--clean-export"])
    if proc.returncode != 0 or (packed / "leftover.txt").exists():
        ok = False
        print(f"    --clean-export 没清掉多出来的文件：\n{proc.stdout}\n{proc.stderr}")

    # 输出目录就是输入文件所在目录时，清空会把源文件一起删掉，必须拒绝
    danger = out / "danger"
    danger.mkdir()
    shutil.copy(ROOT / "tests" / "export" / "config.json", danger / "config.json")
    proc = _export(exe, [str(danger / "config.json"), "--export", str(danger), "--clean-export"])
    if proc.returncode == 0 or not (danger / "config.json").exists():
        ok = False
        print("    源文件就在输出目录里时没拦住清空")

    return ok
