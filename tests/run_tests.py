#!/usr/bin/env python3
"""跑 tests/ 下的自检用例。

一个用例 = tests/<名字>/ 一个目录，里面是它的 shader 场景和 check.py。
check.py 定义 `run(exe, out) -> bool`，用 utils 里的 export()/pixel() 干活，
模块 docstring 的首行当用例名。加用例只要建目录，这个文件不用动。

用法：
    python tests/run_tests.py [--exe build/ShadertoyEmulator.exe] [--keep] [--only 子串]

依赖：Pillow、numpy
"""
import argparse
import importlib.util
import shutil
import sys
import tempfile
from pathlib import Path

# 动态 import check.py 会在每个用例目录里留一堆 __pycache__，把工作区弄脏
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import utils  # noqa: E402  —— 得先把 tests/ 塞进 sys.path 才 import 得到

TESTS_DIR = utils.TESTS_DIR
ROOT = utils.ROOT


def load_case(check_py):
    """import 一个 check.py，返回 (目录名, 用例名, run)"""
    spec = importlib.util.spec_from_file_location(f"check_{check_py.parent.name}", check_py)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    doc = (module.__doc__ or "").strip()
    title = doc.splitlines()[0] if doc else check_py.parent.name
    return check_py.parent.name, title, module.run


def find_cases():
    """扫出所有用例，按目录名排序（顺序稳定，跑起来好对日志）"""
    return [load_case(p) for p in sorted(TESTS_DIR.glob("*/check.py"))]


def main():
    parser = argparse.ArgumentParser(description="跑 tests/ 下的自检用例")
    parser.add_argument("--exe", default="build/ShadertoyEmulator.exe", help="模拟器可执行文件")
    parser.add_argument("--keep", action="store_true", help="保留导出的 PNG 便于排查")
    parser.add_argument("--only", default="", help="只跑目录名里含这个子串的用例")
    args = parser.parse_args()

    exe = (ROOT / args.exe).resolve()
    if not exe.exists():
        print(f"找不到可执行文件：{exe}", file=sys.stderr)
        return 1

    cases = [c for c in find_cases() if args.only in c[0]]
    if not cases:
        print(f"没有匹配的用例（--only {args.only!r}）", file=sys.stderr)
        return 1

    workdir = Path(tempfile.mkdtemp(prefix="shadertoy_tests_"))
    failures = 0
    try:
        for slug, title, run in cases:
            try:
                ok = run(exe, workdir / slug)
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

    print(f"\n{len(cases) - failures}/{len(cases)} 通过")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
