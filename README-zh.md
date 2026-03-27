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
- C++26 编译器
- SFML 3
- glad
- cxxopts
- nlohmann_json
- glslangValidator（可选，用于外部预处理器）

## 构建

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## 使用

### 运行单个着色器

```bash
shadertoy_test shader.glsl
```

### 运行 JSON 配置

```bash
shadertoy_test config.json
```

### 命令行参数

| 参数 | 说明 |
|------|------|
| `--width <n>` | 覆盖窗口宽度 |
| `--height <n>` | 覆盖窗口高度 |
| `--fps` | 在控制台显示帧率 |
| `--builtin-preprocessor` | 使用内置 GLSL 预处理器 |

## 配置文件

创建 `config.json` 配置多通道着色器：

```json
{
  "name": "My Shader",
  "width": 800,
  "height": 600,
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

### iMouse 格式

- `iMouse.xy` = 按下时的鼠标位置
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

**`shaders/` 目录下的所有着色器均直接下载自 [Shadertoy](https://www.shadertoy.com/)，未做任何修改。**

查找原作者和来源：
- 搜索**文件夹名称**（如 "Rainforest"、"wormhole traversal"）
- 或搜索 `config.json` 中的 **`name` 字段**

本项目仅提供模拟器在本地运行这些着色器，所有着色器代码归 Shadertoy 上的原作者所有。

## 交互

- **ESC** - 退出程序
- **鼠标** - 拖拽交互
- **键盘** - 通过 keyboard 通道读取

## 许可证

GNU General Public License v3.0 (GPL-3.0)
