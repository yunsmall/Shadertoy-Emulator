"""shader 编译不过时立刻退出，不接着渲染或导出"""
from utils import export


def run(exe, out):
    try:
        export(exe, ["tests/badshader/image.glsl", "--images", "0:1:1"], out)
    except RuntimeError as exc:
        # 期望的就是失败。除了退出码，还要认一下那句话——否则别的错误（比如
        # GL 上下文建不起来）也会让这个用例通过
        return "Failed to initialize render passes" in str(exc)
    return False
