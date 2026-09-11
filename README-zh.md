# Shadertoy Emulator

一个基于 SFML 3 的 Shadertoy 着色器模拟器，支持在本地运行 Shadertoy 着色器。

## 功能特性

- 支持多通道渲染（BufferA/B/C/D + Image）
- 支持声音着色器（实时音频生成）
- 支持 Common 代码共享
- 支持纹理输入（PNG, JPG, BMP 等）
- 支持键盘输入通道（256x3 纹理，包含按键状态、按下事件、切换状态）
- 支持 `iMouse` 鼠标交互
- 支持 `#include` 等 GLSL 预处理指令
- 支持浮点纹理（GL_RGBA32F）
- 自动双缓冲（自引用 Buffer）
- 可配置纹理过滤（linear、nearest、mipmap）和环绕模式

## 依赖

- CMake 3.20+
- 支持 C++23 的编译器：GCC 13+、Clang 17+ 或 MSVC 19.30+
- Ninja
- SFML 3
- ImGui-SFML
- glad
- cxxopts
- nlohmann_json
- FFmpeg 开发库（libavcodec、libavformat、libavutil、libswscale、libswresample）——视频导出用
- glslangValidator（可选，默认的 GLSL 预处理器，见下方"GLSL 预处理器"）

## 构建

**大部分人不需要构建。** 每次发版都会在
[Releases 页面](https://github.com/yunsmall/Shadertoy-Emulator/releases)附上两个编好的包：

| 平台 | 文件 | 用法 |
|---|---|---|
| Windows | `ShadertoyEmulator-<版本>-win64.zip` | 解压，运行 `ShadertoyEmulator.exe` |
| Linux | `shadertoy-emulator_<版本>_amd64.deb` | `sudo apt install ./shadertoy-emulator_*.deb` |

Windows 包里连 VC++ 运行库都放进去了，解压就能跑。Linux 装完直接执行 `ShadertoyEmulator`
即可，二进制在 `/usr/local/bin`。deb 里写好了对 ffmpeg 等运行库的依赖，`apt` 会一并装上，
所以只适用于 Debian / Ubuntu 系（glibc 2.39 以上，即 Ubuntu 24.04、Debian 13 或更新的版本）。

### 从源码构建

依赖全部走 [vcpkg](https://github.com/microsoft/vcpkg)，用的是**经典模式**——仓库里没有
`vcpkg.json`，包装在 vcpkg 自己的目录下。SFML、ImGui-SFML、glad、cxxopts、nlohmann_json
都由 vcpkg 提供，**不用另外去官网下载**。

1. 装 vcpkg。**放在项目目录外面**，免得被 git 卷进去：

   ```bash
   git clone https://github.com/microsoft/vcpkg ~/vcpkg     # Windows 换成合适的路径
   ~/vcpkg/bootstrap-vcpkg.sh
   # Windows 用 .\vcpkg\bootstrap-vcpkg.bat
   ```

2. 设 `VCPKG_ROOT` 环境变量指向它。构建时那个 toolchain 文件就是靠这个变量定位的：

   ```bash
   # Linux：写进 shell 配置，重开终端后一直有效
   echo 'export VCPKG_ROOT=$HOME/vcpkg' >> ~/.bashrc && source ~/.bashrc
   ```

   ```powershell
   # Windows PowerShell：设完要重开终端
   setx VCPKG_ROOT "E:\path\to\vcpkg"
   ```

3. 装依赖。

   **Windows** —— 完整的 FFmpeg feature 集合（H.264/H.265/VP9/Opus/MP3/AV1）。只要求视频
   导出能用的话，把 ffmpeg 那项换成 `ffmpeg[x264]` 就行，首次构建能省几十分钟。

   ```bash
   vcpkg install sfml imgui-sfml cxxopts nlohmann-json glad[loader] \
                 "ffmpeg[x264,x265,vpx,opus,mp3lame,dav1d]" --triplet x64-windows
   ```

   **Linux** —— 把 `ffmpeg[x264]` 一并交给 vcpkg 最省心（代价是多编几十分钟）。嫌慢的话也
   可以不装 vcpkg 的 ffmpeg，改用发行版的开发库：`CMakeLists.txt` 里 `find_package(FFMPEG)`
   找不到时会自动回退到 pkg-config。

   ```bash
   sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
                    libx11-dev libxrandr-dev libxcursor-dev libxi-dev libxext-dev \
                    libgl1-mesa-dev libudev-dev libasound2-dev
   vcpkg install sfml imgui-sfml cxxopts nlohmann-json glad[loader] --triplet x64-linux
   ```

4. 配置与构建。`-DCMAKE_TOOLCHAIN_FILE` 不能省——vcpkg 就是靠它把这些包喂给 `find_package`。

   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
   cmake --build build --parallel 6
   ```

5. 运行。Windows 上还要能找到运行时 DLL，二选一：

   ```powershell
   # 当前终端临时生效
   $env:PATH = "$env:VCPKG_ROOT\installed\x64-windows\bin;$env:PATH"
   .\build\ShadertoyEmulator.exe shader.glsl
   ```

   或者把 `installed\x64-windows\bin` 下的 DLL 拷到 exe 旁边。

   ```bash
   # Linux
   ./build/ShadertoyEmulator shader.glsl
   ```

### 构建报错对照

| 报错 | 多半是 |
|---|---|
| `Could not find a package configuration file provided by "SFML"` | 没传 `-DCMAKE_TOOLCHAIN_FILE`，或 `VCPKG_ROOT` 没设 / 指错了 |
| `Could not find "vcpkg.cmake"` | `VCPKG_ROOT` 指错，或者 vcpkg 没 bootstrap 过 |
| 装依赖时提示要用 `--recurse` | vcpkg 里已经装过这个包的旧版本，按提示加上 `--recurse` 重装 |
| Windows 上双击 exe 闪退 | 缺运行时 DLL，见上面第 5 步 |

### 测试

需要 Python 3 和 Pillow；视频用例还需要 `ffprobe` 在 `PATH` 里。可执行文件默认按
Windows 的路径找，所以 Linux 上要显式指定：

```bash
python tests/run_tests.py                                  # Windows
python tests/run_tests.py --exe build/ShadertoyEmulator    # Linux
```

## 使用

### 运行单个着色器

```bash
ShadertoyEmulator shader.glsl
```

### 运行 JSON 配置

```bash
ShadertoyEmulator config.json
```

### 命令行参数

| 参数 | 说明 |
|------|------|
| `--width <n>` | 覆盖窗口宽度 |
| `--height <n>` | 覆盖窗口高度 |
| `--show-fps` | 在控制台显示帧率 |
| `--gui` / `--no-gui` | 强制开启 / 关闭 ImGui 面板（覆盖配置） |
| `--images <start:stop:step>` | **图片模式**：导出 PNG 序列，Python 切片语法，如 `0:300:2`（`stop` 必填且不含） |
| `--output-dir <dir>` | PNG 保存目录，用 `--images` 时必填 |
| `--video <file.mp4>` | **视频模式**：导出 H.264 + AAC 的 mp4，需搭配 `--duration` |
| `--duration <秒>` | 视频时长，用 `--video` 时必填 |
| `--fps <n>` | 导出帧率（默认 60）：图片模式决定 `iTime` 的步长，视频模式还决定输出帧率 |
| `--dump-audio <file.wav>` | 额外把 Sound pass 的输出写成 WAV |
| `--builtin-preprocessor` | 使用内置 GLSL 预处理器 |

三种模式互斥：不给 `--images` 或 `--video` 就是窗口模式。导出模式没有窗口、没有 ImGui、
没有键鼠输入，跑完自动退出。

## GLSL 预处理器

默认使用外部预处理器 `glslangValidator`（以 `-S frag -E` 调用），需要自行安装并加入 `PATH`。

### 安装

| 平台 | 方式 |
|------|------|
| Windows | 从 [glslang releases](https://github.com/KhronosGroup/glslang/releases) 下载压缩包，把其中的 `bin` 目录加进 `PATH`；或安装 Vulkan SDK（自带）。vcpkg 用户要装 `tools` feature：`vcpkg install glslang[tools]`（不带这个 feature 只有库，没有可执行文件），装完在 `installed/x64-windows/tools/glslang/` |
| Linux | `apt install glslang-tools`（Debian/Ubuntu）、`pacman -S glslang`（Arch） |
| macOS | `brew install glslang` |

装完用 `glslangValidator --version` 确认。

### 不装也能用

程序启动时扫一遍 `PATH`，找不到 `glslangValidator` 会自动改用内置预处理器，并打印一行提示：

```
glslangValidator not found in PATH, falling back to built-in preprocessor
```

也可以显式指定：

```bash
ShadertoyEmulator config.json --builtin-preprocessor
```

内置预处理器支持 `#include`（相对当前文件解析，自动检测循环引用）、`#define`（含带参数的宏）、`#undef`、`#ifdef` / `#ifndef` / `#else` / `#endif`，对同一份代码的结果与外部预处理器一致（`tests/preprocessor/` 就是拿两者互相对照的）。

## 配置文件

创建 `config.json` 配置多通道着色器：

```json
{
  "name": "My Shader",
  "width": 1280,
  "height": 720,
  "common": "common.glsl",
  "passes": [
    {
      "name": "BufferA",
      "shader": "buffera.glsl",
      "width": 512,
      "height": 512,
      "channels": {
        "0": { "type": "buffer", "source": "BufferA" },
        "1": { "type": "keyboard" }
      }
    },
    {
      "name": "Image",
      "shader": "image.glsl",
      "channels": {
        "0": { "type": "buffer", "source": "BufferA" },
        "1": { "type": "texture", "source": "noise.png", "filter": "nearest", "wrap": "repeat" }
      }
    }
  ]
}
```

详细配置说明请参阅 [shaders/JSON_CONFIG.md](shaders/JSON_CONFIG.md)。

## 导出

### 图片序列

```bash
ShadertoyEmulator config.json --images 0:300:2 --output-dir frames/ --fps 30
```

- `--images` 用 Python 切片语法：`0:300:2` 表示保存第 0、2、4、…、298 帧（`stop` 不含且必填）
- 文件名为 `%05d.png`，用绝对帧号（`00000.png`、`00002.png`…）
- 导出时用虚拟时间（`iTime = iFrame / --fps`），保证结果可复现
- 输出目录不存在会自动创建

### 视频

```bash
ShadertoyEmulator config.json --video out.mp4 --duration 5 --fps 60
```

- 从第 0 帧开始，导出 `--duration` 秒
- 编码 H.264 视频 + AAC 音频封装成 mp4；shader 有 Sound pass 就自动带音轨
- 宽高必须是偶数（H.264 用的 YUV420P 要求），奇数尺寸会直接报错而不是悄悄裁掉一列
- 音频按视频时间轴生成，音画严格同步

编码器来自 FFmpeg，且必须是带 libx264 编译的（FFmpeg 唯一的 H.264 编码器就是它）：

| 平台 | 方式 |
|------|------|
| Windows | `vcpkg install ffmpeg[x264,x265,vpx,opus,mp3lame,dav1d]`，CMake 会自动找到 vcpkg 的 `FindFFMPEG` 模块 |
| Debian/Ubuntu | `apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev`，走 pkg-config |
| macOS | `brew install ffmpeg` |

两种导出模式都可以加 `--dump-audio out.wav`，额外把 Sound pass 的输出写成 WAV。

## Shadertoy 兼容性

### 支持的 Uniform 变量

| 变量 | 类型 | 说明 |
|------|------|------|
| `iResolution` | vec3 | 窗口/缓冲区分辨率 |
| `iTime` | float | 运行时间（秒） |
| `iTimeDelta` | float | 帧间隔时间 |
| `iFrame` | int | 帧计数 |
| `iFrameRate` | float | 帧率 |
| `iMouse` | vec4 | 鼠标状态 |
| `iDate` | vec4 | 日期时间 |
| `iChannel0-3` | sampler2D | 输入通道 |
| `iChannelResolution` | vec3[4] | 通道分辨率 |

### 坐标系与方向

所有像素坐标都和 Shadertoy / OpenGL 一致：**原点在左下角，y 轴向上**。

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;   // 左下 (0,0)，右上 (1,1)
    fragColor = vec4(uv, 0.0, 1.0);         // 左下黑，右上黄
}
```

| 量 | 坐标系 |
|---|---|
| `fragCoord` | 当前 pass 的渲染目标，左下原点，y 向上 |
| `iResolution.xy` | 当前 pass 的渲染目标尺寸：Image 通道是窗口尺寸，Buffer 通道是 config 里写的 `width`/`height`（省略则等于窗口） |
| `iMouse.xy` | **窗口**像素，左下原点，y 向上。与 Image 通道的 `fragCoord` 同尺度；Buffer 通道分辨率与窗口不同时需自行按比例换算 |
| 纹理 `iChannel0-3` | `texture()` 的 uv 与 `fragCoord/iResolution` 同向，但归一化到该纹理自己的尺寸；像素尺寸见 `iChannelResolution[i]` |
| 键盘纹理 | y=0/1/2 三行，见"键盘纹理格式" |

**纹理方向**：`type: "texture"` 默认（`flipY: false`）会让图片上下颠倒——`texture(iChannel0, vec2(0.5, 0.0))` 取到的是图片**顶边**那一行。想让 `uv` 和看图软件里的方向一致，加 `"flipY": true`，此时 `uv.y` 越大越靠图片上方：

```json
"0": { "type": "texture", "source": "photo.png", "flipY": true }
```

**导出 PNG**：`--images` 保存的 PNG 已经翻正，和 shader 里 `uv` 的方向一致（`uv.y` 大的位置在 PNG 上方），不用再自己翻转。

**导出模式**：没有输入，`iMouse` 恒为 `vec4(0)`，键盘纹理全 0。

### iMouse 格式

- `iMouse.xy` = 按住时的鼠标位置（拖拽中实时更新，松开后停在最后位置）
- `abs(iMouse.zw)` = 点击时的鼠标位置
- `sign(iMouse.z)` = 按钮是否按下（正=按下）
- `sign(iMouse.w)` = 是否刚点击（正=刚点击）

### 键盘纹理格式

`type: "keyboard"` 通道提供一个 256x3 像素的纹理：

- 第一行 (y=0): 当前帧按键状态（keydown）
- 第二行 (y=1): 按键刚按下事件（keypressed）
- 第三行 (y=2): 按键切换状态（每次按键切换）

```glsl
// 检测 A 键是否按下
float aPressed = texelFetch(iChannel0, ivec2(65, 0), 0).x;

// 检测 Space 键是否刚按下
float spaceJustPressed = texelFetch(iChannel0, ivec2(32, 1), 0).x;

// 检测切换状态
float toggleState = texelFetch(iChannel0, ivec2(65, 2), 0).x;
```

常用键码（JavaScript keyCode）：A-Z (65-90), 0-9 (48-57), Space (32), 方向键 (37-40), F1-F12 (112-123)

### 声音着色器

声音通道实时生成音频。定义 `mainSound` 函数：

```glsl
vec2 mainSound(int samp, float time) {
    // 返回 vec2(左声道, 右声道)，范围 -1.0 到 1.0
    float wave = sin(time * 440.0 * 6.28318) * 0.5;
    return vec2(wave, wave);
}
```

声音着色器支持 `iSampleRate`（44100）、`iSampleOffset`，并可读取 Buffer 通道实现音画同步。

## 示例着色器

`shaders/` 目录下包含多个示例：

- `Rainforest/` - 雨林效果
- `wormhole traversal/` - 虫洞穿越效果
- `blackhole/` - 黑洞效果
- `Rainforest/` - 雨林效果
- `EscapeTheGamegrid/` - 游戏效果

## 版权声明

`shaders/` 目录下的着色器均来自 [Shadertoy](https://www.shadertoy.com/)。**一律保持原样：凡有改动，必定只是修
bug，绝不改变原有功能**，且每一处都会写在该着色器 `config.json` 的 `description` 字段里。

查找原作者和来源：
- 搜索**文件夹名称**（如 "Rainforest"、"wormhole traversal"）
- 或搜索 `config.json` 中的 **`name` 字段**
- 若该着色器的 `config.json` 里填了 **`url` 字段**，直接访问即可

本项目仅提供模拟器在本地运行这些着色器，所有着色器代码归 Shadertoy 上的原作者所有。

## 交互

- **ESC** - 退出程序
- **鼠标** - 拖拽交互
- **键盘** - 通过 keyboard 通道读取

## 许可证

GNU General Public License v3.0 (GPL-3.0)
