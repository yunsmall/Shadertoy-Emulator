# Shadertoy Emulator - JSON 配置文件文档

## 完整配置示例

```json
{
  "name": "Fractal Noise",
  "width": 1280,
  "height": 720,
  "common": "common.glsl",
  "passes": [
    {
      "name": "BufferA",
      "shader": "buffer_a.glsl",
      "width": 512,
      "height": 512,
      "channels": {
        "0": { "type": "texture", "source": "textures/noise.png" },
        "1": { "type": "buffer", "source": "BufferA" }
      }
    },
    {
      "name": "BufferB",
      "shader": "buffer_b.glsl",
      "channels": {
        "0": { "type": "buffer", "source": "BufferA" }
      }
    },
    {
      "name": "Image",
      "shader": "image.glsl",
      "channels": {
        "0": { "type": "buffer", "source": "BufferA" },
        "1": { "type": "buffer", "source": "BufferB" },
        "2": { "type": "texture", "source": "textures/color_palette.png" }
      }
    }
  ]
}
```

## 顶层字段

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `name` | string | 否 | "Shadertoy Emulator" | 窗口标题和shader名称 |
| `width` | int | 否 | 800 | 窗口宽度（像素） |
| `height` | int | 否 | 600 | 窗口高度（像素） |
| `common` | string | 否 | - | 共享GLSL代码文件路径（相对于config.json） |
| `passes` | array | **是** | - | 渲染通道列表，按顺序执行 |

## passes[] 通道配置

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `name` | string | **是** | - | 通道名称，必须是 `BufferA`/`BufferB`/`BufferC`/`BufferD` 或 `Image` |
| `shader` | string | **是** | - | GLSL shader文件路径（相对于config.json） |
| `width` | int | 否 | 窗口宽度 | 该通道渲染目标宽度 |
| `height` | int | 否 | 窗口高度 | 该通道渲染目标高度 |
| `channels` | object | 否 | {} | 输入通道配置，键为 "0"-"3" |

## channels 输入通道配置

| 字段 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `type` | string | **是** | - | 输入类型：`"texture"`、`"buffer"` 或 `"keyboard"` |
| `source` | string | 条件 | - | 纹理文件路径或Buffer名称（texture/buffer必填） |
| `filter` | string | 否 | `"linear"` | 纹理过滤：`"linear"`、`"nearest"` 或 `"mipmap"` |
| `wrap` | string | 否 | `"clamp"` | 纹理环绕：`"clamp"`、`"repeat"` 或 `"mirror"` |
| `flipY` | bool | 否 | `false` | 是否上下翻转纹理（仅texture类型） |

### type 类型说明

| 类型 | 说明 |
|------|------|
| `"texture"` | 从图片文件加载纹理（支持 PNG, JPG, BMP 等） |
| `"buffer"` | 引用另一个Buffer通道的渲染输出 |
| `"keyboard"` | 键盘输入纹理（256x2像素，无需source） |

### keyboard 纹理格式

键盘纹理为 256x3 像素的纹理：
- **第一行 (y=0)**: 当前帧按键状态 - `texelFetch(channel, ivec2(keyCode, 0), 0).x` = 1.0 表示按键被按住
- **第二行 (y=1)**: 按键刚按下事件 - `texelFetch(channel, ivec2(keyCode, 1), 0).x` = 1.0 表示本帧刚按下
- **第三行 (y=2)**: 按键切换状态 - 每次按键切换，可用于开关功能

常用键码（JavaScript keyCode 标准）：
- A-Z: 65-90
- 0-9: 48-57
- Space: 32
- Enter: 13
- Backspace: 8
- Tab: 9
- Escape: 27
- 方向键: 37-40 (左/上/右/下)
- F1-F12: 112-123

### filter 过滤模式

| 值 | 说明 |
|------|------|
| `"linear"` | 双线性过滤（默认，适合大多数情况） |
| `"nearest"` | 最近邻过滤（像素风格、需要精确采样） |
| `"mipmap"` | 三线性过滤 + mipmap（适合需要 LOD 的纹理，如噪声纹理） |

### wrap 环绕模式

| 值 | 说明 |
|------|------|
| `"clamp"` | 边缘拉伸（默认） |
| `"repeat"` | 平铺重复 |
| `"mirror"` | 镜像重复 |

### 通道配置示例

```json
{
  "0": { "type": "keyboard" },
  "1": { "type": "buffer", "source": "BufferA" },
  "2": { "type": "texture", "source": "noise.png", "filter": "nearest", "wrap": "repeat" },
  "3": { "type": "texture", "source": "font.png", "flipY": true }
}
```

## 通道命名规则

| 名称 | 用途 |
|------|------|
| `BufferA` | 第一个缓冲通道 |
| `BufferB` | 第二个缓冲通道 |
| `BufferC` | 第三个缓冲通道 |
| `BufferD` | 第四个缓冲通道 |
| `Image` | 最终输出通道（渲染到屏幕） |
| `Sound` | 声音输出通道（生成音频） |

## Sound 声音通道

声音通道用于生成音频输出，函数签名为：

```glsl
vec2 mainSound(int samp, float time) {
    // samp: 采样索引（从0开始递增）
    // time: 当前时间（秒）
    // 返回: vec2(左声道, 右声道)，范围 -1.0 到 1.0
    return vec2(wave, wave);
}
```

### 声音着色器示例

```glsl
// 440Hz 正弦波
vec2 mainSound(int samp, float time) {
    float freq = 440.0;
    float wave = sin(time * freq * 6.28318) * 0.5;
    return vec2(wave, wave);
}
```

### 声音通道配置

```json
{
  "passes": [
    {
      "name": "Sound",
      "shader": "sound.glsl"
    },
    {
      "name": "Image",
      "shader": "image.glsl"
    }
  ]
}
```

### 声音着色器可用的 uniform

| Uniform | 说明 |
|---------|------|
| `iSampleRate` | 采样率（44100） |
| `iSampleOffset` | 当前采样偏移 |
| `iTime` | 当前时间（秒） |
| `iResolution` | 缓冲区分辨率 |
| `iChannel0-3` | 输入通道纹理 |
| `iMouse` | 鼠标状态 |
| `iDate` | 日期时间 |

> 声音着色器可以读取 Buffer 通道的数据，实现音画同步。

## 文件路径规则

所有路径都**相对于 config.json 所在目录**：

```
shaders/my_effect/
├── config.json          # 配置文件
├── common.glsl          # "common": "common.glsl"
├── buffer_a.glsl        # "shader": "buffer_a.glsl"
├── image.glsl
└── textures/
    └── noise.png        # "source": "textures/noise.png"
```

## 典型配置模式

### 1. 单通道shader（最简单）

```json
{
  "name": "Simple Shader",
  "width": 800,
  "height": 600,
  "passes": [
    {
      "name": "Image",
      "shader": "image.glsl"
    }
  ]
}
```

### 2. 反馈效果（Buffer引用自己）

```json
{
  "passes": [
    {
      "name": "BufferA",
      "shader": "buffer_a.glsl",
      "channels": {
        "0": { "type": "buffer", "source": "BufferA" }
      }
    },
    {
      "name": "Image",
      "shader": "image.glsl",
      "channels": {
        "0": { "type": "buffer", "source": "BufferA" }
      }
    }
  ]
}
```

> 当Buffer引用自己时，系统自动启用双缓冲

### 3. 多Buffer串联

```json
{
  "passes": [
    {
      "name": "BufferA",
      "shader": "buffer_a.glsl"
    },
    {
      "name": "BufferB",
      "shader": "buffer_b.glsl",
      "channels": {
        "0": { "type": "buffer", "source": "BufferA" }
      }
    },
    {
      "name": "Image",
      "shader": "image.glsl",
      "channels": {
        "0": { "type": "buffer", "source": "BufferA" },
        "1": { "type": "buffer", "source": "BufferB" }
      }
    }
  ]
}
```

### 4. 使用纹理输入

```json
{
  "passes": [
    {
      "name": "Image",
      "shader": "image.glsl",
      "channels": {
        "0": { "type": "texture", "source": "textures/input.png" },
        "1": { "type": "texture", "source": "textures/normal_map.png" }
      }
    }
  ]
}
```

## Common 代码文件

`common.glsl` 用于定义所有通道共享的函数和常量：

```glsl
// common.glsl
#ifndef COMMON_GLSL
#define COMMON_GLSL

#define PI 3.14159265359
#define TAU (2.0 * PI)
#define SAT(x) clamp(x, 0.0, 1.0)

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

#endif
```

Common代码会自动注入到所有通道的shader中。

## GLSL 预处理器

默认使用外部预处理器 `glslangValidator -S frag -E`，需要安装并添加到PATH。

如果未安装 glslangValidator，可使用 `--builtin-preprocessor` 参数切换到内置预处理器。

内置预处理器支持以下指令：

| 指令 | 说明 |
|------|------|
| `#include "file.glsl"` | 包含其他GLSL文件（支持嵌套，自动防止循环引用） |
| `#define MACRO value` | 定义简单宏 |
| `#define MACRO(args) body` | 定义带参数的宏 |
| `#undef MACRO` | 取消宏定义 |
| `#ifdef MACRO` | 如果宏已定义 |
| `#ifndef MACRO` | 如果宏未定义 |
| `#else` | 否则分支 |
| `#endif` | 结束条件块 |

### 使用示例

**common.glsl**:
```glsl
#ifndef COMMON_GLSL
#define COMMON_GLSL

#define PI 3.14159265359
#define SAT(x) clamp(x, 0.0, 1.0)

// 包含其他文件
#include "utils/noise.glsl"

#endif
```

**buffer_a.glsl**:
```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;

    // 使用 common 中定义的宏
    float x = SAT(uv.x);

    fragColor = vec4(vec3(x), 1.0);
}
```

### 注意事项

1. `#include` 路径相对于当前文件所在目录
2. 循环引用会被自动检测并跳过
3. 宏展开支持递归（宏定义中使用其他宏）
4. 使用 `#ifndef` / `#define` / `#endif` 防止重复包含

## 渲染顺序

通道按 `passes` 数组顺序依次渲染：

1. BufferA → 渲染到离屏纹理
2. BufferB → 渲染到离屏纹理（可读取BufferA）
3. BufferC → 渲染到离屏纹理（可读取BufferA/B）
4. BufferD → 渲染到离屏纹理（可读取BufferA/B/C）
5. Image → 渲染到屏幕（可读取所有Buffer）

## 注意事项

1. **循环依赖**：避免 A→B→A 这样的循环引用，会导致未定义行为
2. **分辨率**：Buffer可设置独立分辨率，用于优化性能或实现特定效果
3. **双缓冲**：自引用Buffer自动启用双缓冲，无需额外配置
4. **路径分隔符**：支持 `/` 和 `\`，建议统一使用 `/`

## 命令行使用

```bash
# 运行JSON配置
shadertoy_test config.json

# 覆盖分辨率
shadertoy_test config.json --width 1920 --height 1080

# 显示FPS
shadertoy_test config.json --fps

# 使用内置预处理器（默认使用外部 glslangValidator）
shadertoy_test config.json --builtin-preprocessor
```

### 命令行参数

| 参数 | 说明 |
|------|------|
| `--width <n>` | 覆盖窗口宽度 |
| `--height <n>` | 覆盖窗口高度 |
| `--fps` | 在控制台显示帧率 |
| `--builtin-preprocessor` | 使用内置GLSL预处理器（默认使用外部glslangValidator） |
