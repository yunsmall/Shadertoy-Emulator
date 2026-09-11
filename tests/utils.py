"""用例共用的工具：跑导出、读像素。

check.py 里 `from utils import export, pixel` 就行。
"""
import subprocess
import sys
from pathlib import Path

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


def pixel_rgba(png, xr, yr):
    """同 pixel()，但保留 alpha——验通道数据用的就是它"""
    im = Image.open(png).convert("RGBA")
    w, h = im.size
    return im.getpixel((min(int(w * xr), w - 1), min(int(h * yr), h - 1)))


def export(exe, args, outdir, with_output_dir=True):
    """跑一次导出。视频模式自带输出路径，不接受 --output-dir。

    args 里的路径按仓库根目录解析（子进程的工作目录就是那里）。
    """
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
