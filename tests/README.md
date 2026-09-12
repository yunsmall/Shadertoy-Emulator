# 自检用例

一个用例 = `tests/<名字>/` 一个目录，里面是它的最小 shader 场景，把要验证的量编码进像素值，
外加一个 `check.py` 做断言（导出后用 PIL 读回来核对）。

## 运行

```bash
python tests/run_tests.py                          # 默认用 build/ShadertoyEmulator.exe
python tests/run_tests.py --exe cmake-build-release/ShadertoyEmulator.exe
python tests/run_tests.py --keep                    # 保留导出的 PNG 便于排查
python tests/run_tests.py --only alpha              # 只跑目录名含 alpha 的用例
```

依赖 Pillow，视频用例另外需要 `ffprobe`。导出结果默认写到临时目录，跑完即删。

## 用例

| 目录 | 验证内容 |
|---|---|
| `selftest/` | 单 pass：`iFrame` / `iTime` / `iDate` 是否正确传入，以及图像上下方向 |
| `images/` | 图片序列导出：帧范围、步长、文件名，以及中间帧确实渲染过（用 `selftest/` 的场景） |
| `skipintermediate/` | 导出图片默认跳过无状态的中间帧（结果与全渲染逐像素一致），`--force-skip-intermediate` 连有状态的一起跳（于是不一致） |
| `multipass/` | Buffer 与 Image 两个 pass 看到的 `iFrame` 必须一致（曾差一整帧） |
| `passres/` | Buffer 独立分辨率：buffer 内看到的 `iResolution` 应是它自己的，而非窗口的 |
| `texture/` | 文件纹理通道：加载、采样值，以及 `flipY` 的两个方向 |
| `feedback/` | 自引用 Buffer（双缓冲）：帧 N 的累加值应为 N+1 |
| `alphachan/` | Buffer 的 alpha 是数据（帧间累加），Image 的 alpha 被包装钉成 1 |
| `mipmap/` | 自引用 Buffer 配 `filter: mipmap` 时，高 mip 级能采到颜色而不是黑（黑说明 mipmap 链没生成） |
| `crossref/` | 两个 Buffer 互相引用：BufferA = 2N+1，BufferB = 2N+2 |
| `ring3/` | 三节点成环引用：A=3N+1、B=3N+2、C=3N+3（环里第一个渲染的读上一帧，其余读本帧） |
| `ring4/` | 四节点成环引用：A=4N+1 … D=4N+4，环再长规律不变 |
| `selfref_read/` | 自引用的 Buffer 被别的 pass 读取时给的是本帧的值（B = 10×(N+1)，不是 10×N） |
| `sound/` | Sound pass 左右声道各几个已知频率，`--dump-audio` 导出后校验频率、幅度和声道隔离 |
| `sound_common/` | Sound 引用 common 里的函数，公共代码只能展开一次（用 `sound/` 的场景） |
| `preprocessor/` | 宏、参数宏、条件编译、`#undef`、`#include`，内置与外部预处理器结果必须一致（外部需要 `glslangValidator`） |
| `zeroinit/` | 未初始化的变量被补成 0（内置类型、多声明、已有初值的不动），struct 成员和 uniform 不能被误加初值 |
| `video/` | 视频导出的帧数、时长和音轨，用 `ffprobe` 核对，并确认音频时长跟着视频走而不是按每帧 0.5 秒累积（用 `sound/` 的场景） |

有几个用例共用 `sound/` 和 `selftest/` 里的场景，那些 shader 只存一份。

## 加新用例

1. `mkdir tests/<名字>/`，把 `config.json` 和 shader 放进去（单 pass 的话直接放一个 `.glsl`）
2. 在里面写个 `check.py`：模块 docstring 的首行当用例名，`run(exe, out)` 返回 `True`/`False`
3. 没了——`run_tests.py` 自己扫 `tests/*/check.py`，不用登记

```python
"""一句话说明这个用例验什么"""
from utils import export, pixel


def run(exe, out):
    export(exe, ["tests/<名字>/config.json", "--images", "0:5:1"], out)
    got = pixel(out / "00000.png", 0.5, 0.5)[0]
    if got != 128:
        print(f"    读回 {got}，期望 128")
        return False
    return True
```

`utils` 里是三个工具：`export()` 跑一次导出（失败会抛异常，异常信息带退出码和输出）、
`pixel()` / `pixel_rgba()` 按相对坐标读像素（左上角为原点，后者保留 alpha）。
`out` 是这个用例专属的临时目录。shader 和 config 的路径按**仓库根目录**写。

shader 里把待验证的量编码成颜色时，注意导出的是 8 位 PNG（值域 0–255，会钳位），
且 `glReadPixels` 的结果已做上下翻转，所以 shader 里 `uv.y` 大的位置对应 PNG 的上方。
