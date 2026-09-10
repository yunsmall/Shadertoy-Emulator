# 自检用例

每个用例是一个最小 shader 场景，把要验证的量编码进像素值，导出后用 `run_tests.py` 读回来核对。

## 运行

```bash
python tests/run_tests.py                          # 默认用 build/ShadertoyEmulator.exe
python tests/run_tests.py --exe cmake-build-release/ShadertoyEmulator.exe
python tests/run_tests.py --keep                    # 保留导出的 PNG 便于排查
```

依赖 Pillow，视频用例另外需要 `ffprobe`。导出结果默认写到临时目录，跑完即删。

## 用例

| 目录 | 验证内容 |
|---|---|
| `selftest.glsl` | 单 pass：`iFrame` / `iTime` / `iDate` 是否正确传入，以及图像上下方向 |
| `multipass/` | Buffer 与 Image 两个 pass 看到的 `iFrame` 必须一致（曾差一整帧） |
| `feedback/` | 自引用 Buffer（双缓冲）：帧 N 的累加值应为 N+1 |
| `mipmap/` | 自引用 Buffer 配 `filter: mipmap` 时，高 mip 级能采到颜色而不是黑（黑说明 mipmap 链没生成） |
| `crossref/` | 两个 Buffer 互相引用：BufferA = 2N+1，BufferB = 2N+2 |
| `ring3/` | 三节点成环引用：A=3N+1、B=3N+2、C=3N+3（环里第一个渲染的读上一帧，其余读本帧） |
| `ring4/` | 四节点成环引用：A=4N+1 … D=4N+4，环再长规律不变 |
| `selfref_read/` | 自引用的 Buffer 被别的 pass 读取时给的是本帧的值（B = 10×(N+1)，不是 10×N） |
| `sound/` | Sound pass 输出 440Hz 正弦波，用 `--dump-audio` 导出后校验频率和幅度 |
| `preprocessor/` | 宏、参数宏、条件编译、`#undef`、`#include`，内置与外部预处理器结果必须一致（外部需要 `glslangValidator`） |
| `video/`（用 `sound/` 的配置） | 视频导出的帧数、时长和音轨，用 `ffprobe` 核对，并确认音频时长跟着视频走而不是按每帧 0.5 秒累积 |

## 加新用例

1. 在 `tests/<名字>/` 下放 `config.json` 和 shader（单 pass 的话直接放一个 `.glsl`）
2. 在 `run_tests.py` 里写一个 `case_xxx(exe, out)` 函数，返回 `True`/`False`
3. 加进 `CASES` 列表

shader 里把待验证的量编码成颜色时，注意导出的是 8 位 PNG（值域 0–255，会钳位），
且 `glReadPixels` 的结果已做上下翻转，所以 shader 里 `uv.y` 大的位置对应 PNG 的上方。
