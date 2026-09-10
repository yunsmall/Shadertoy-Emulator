# Shadertoy Emulator

A Shadertoy shader emulator based on SFML 3, allowing you to run Shadertoy shaders locally.

## Features

- Multi-pass rendering support (BufferA/B/C/D + Image)
- Sound shader support (real-time audio generation)
- Common code sharing
- Texture input support (PNG, JPG, BMP, etc.)
- Keyboard input channel (256x3 texture with key state, press event, and toggle)
- `iMouse` mouse interaction
- GLSL preprocessor directives including `#include`
- Floating-point textures (GL_RGBA32F)
- Automatic double buffering for self-referencing buffers
- Configurable texture filtering (linear, nearest, mipmap) and wrapping modes

## Dependencies

- CMake 3.20+
- C++26 compiler
- SFML 3
- glad
- cxxopts
- nlohmann_json
- FFmpeg dev libraries (libavcodec, libavformat, libavutil, libswscale, libswresample) — for video export
- glslangValidator (optional, the default GLSL preprocessor — see "GLSL Preprocessor" below)

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

### Run a single shader

```bash
ShadertoyEmulator shader.glsl
```

### Run with JSON configuration

```bash
ShadertoyEmulator config.json
```

### Command Line Arguments

| Argument | Description |
|----------|-------------|
| `--width <n>` | Override window width |
| `--height <n>` | Override window height |
| `--show-fps` | Display frame rate in console |
| `--gui` / `--no-gui` | Force-enable / disable the ImGui panel (overrides config) |
| `--images <start:stop:step>` | **Image mode**: export a PNG sequence, Python slice syntax, e.g. `0:300:2` (`stop` required and excluded) |
| `--output-dir <dir>` | Directory for the PNG sequence; required with `--images` |
| `--video <file.mp4>` | **Video mode**: export an H.264 + AAC mp4; requires `--duration` |
| `--duration <seconds>` | Length of the exported video; required with `--video` |
| `--fps <n>` | Export frame rate (default: 60). Image mode uses it as the `iTime` step; video mode also uses it as the output frame rate |
| `--dump-audio <file.wav>` | Also write the Sound pass output to a WAV file |
| `--builtin-preprocessor` | Use built-in GLSL preprocessor |

The three modes are mutually exclusive: without `--images` or `--video` you get window mode.
Export modes have no window, no ImGui and no keyboard/mouse input, and exit when done.

## GLSL Preprocessor

The default preprocessor is the external `glslangValidator` (invoked as `-S frag -E`). It must be installed and on your `PATH`.

### Installation

| Platform | How |
|----------|-----|
| Windows | Download a release from [glslang releases](https://github.com/KhronosGroup/glslang/releases) and add its `bin` directory to `PATH`; or install the Vulkan SDK, which bundles it. vcpkg users need the `tools` feature: `vcpkg install glslang[tools]` (without it you only get the library, no executable), which puts the binary in `installed/x64-windows/tools/glslang/` |
| Linux | `apt install glslang-tools` (Debian/Ubuntu), `pacman -S glslang` (Arch) |
| macOS | `brew install glslang` |

Verify with `glslangValidator --version`.

### Without It

The program scans `PATH` at startup. If `glslangValidator` is missing it falls back to the built-in preprocessor automatically and prints:

```
glslangValidator not found in PATH, falling back to built-in preprocessor
```

You can also ask for it explicitly:

```bash
ShadertoyEmulator config.json --builtin-preprocessor
```

It supports `#include` (resolved relative to the current file, with cycle detection), `#define` (including function-like macros), `#undef`, and `#ifdef` / `#ifndef` / `#else` / `#endif`, and produces the same result as the external one for the same input (`tests/preprocessor/` cross-checks the two).

## Configuration

Create a `config.json` to configure multi-pass shaders:

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

For detailed configuration, see [shaders/JSON_CONFIG.md](shaders/JSON_CONFIG.md).

## Export

### Image Sequence

```bash
ShadertoyEmulator config.json --images 0:300:2 --output-dir frames/ --fps 30
```

- `--images` uses Python slice syntax: `0:300:2` saves frames 0, 2, 4, …, 298 (`stop` is excluded and required).
- Files are named `%05d.png` with the absolute frame index (`00000.png`, `00002.png`, …).
- Export uses virtual time (`iTime = iFrame / --fps`) so results are reproducible.
- The output directory is created if it does not exist.

### Video

```bash
ShadertoyEmulator config.json --video out.mp4 --duration 5 --fps 60
```

- Starts at frame 0 and exports `--duration` seconds.
- Encodes H.264 video plus AAC audio into an mp4; if the shader has a Sound pass the audio track is added automatically.
- Width and height must be even (H.264 uses YUV420P); odd sizes are rejected instead of silently dropping a column.
- Audio is generated along the video timeline, so picture and sound stay in sync.

The encoder comes from FFmpeg, which must be built with libx264 (the only H.264 encoder FFmpeg has):

| Platform | How |
|----------|-----|
| Windows | `vcpkg install ffmpeg[x264,x265,vpx,opus,mp3lame,dav1d]`; CMake picks up vcpkg's `FindFFMPEG` module automatically |
| Debian/Ubuntu | `apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev`; found through pkg-config |
| macOS | `brew install ffmpeg` |

Both export modes accept `--dump-audio out.wav` to additionally write the Sound pass output.

## Shadertoy Compatibility

### Supported Uniforms

| Variable | Type | Description |
|----------|------|-------------|
| `iResolution` | vec3 | Window/buffer resolution |
| `iTime` | float | Running time (seconds) |
| `iTimeDelta` | float | Frame delta time |
| `iFrame` | int | Frame count |
| `iFrameRate` | float | Frame rate |
| `iMouse` | vec4 | Mouse state |
| `iDate` | vec4 | Date and time |
| `iChannel0-3` | sampler2D | Input channels |
| `iChannelResolution` | vec3[4] | Channel resolutions |

### Coordinate System and Orientation

All pixel coordinates follow Shadertoy / OpenGL: **origin at the bottom-left, y axis pointing up**.

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;   // (0,0) bottom-left, (1,1) top-right
    fragColor = vec4(uv, 0.0, 1.0);         // black bottom-left, yellow top-right
}
```

| Value | Coordinate system |
|---|---|
| `fragCoord` | Current pass's render target, bottom-left origin, y up |
| `iResolution.xy` | Current pass's render target size: window size for the Image pass, the `width`/`height` from config for Buffer passes (falls back to window size) |
| `iMouse.xy` | **Window** pixels, bottom-left origin, y up. Same scale as `fragCoord` in the Image pass; Buffer passes with a different resolution must rescale it themselves |
| Texture `iChannel0-3` | `texture()` uv runs the same way as `fragCoord/iResolution`, but normalized to that texture's own size; pixel size is in `iChannelResolution[i]` |
| Keyboard texture | Rows y=0/1/2, see "Keyboard Texture Format" |

**Texture orientation**: `type: "texture"` with the default (`flipY: false`) flips the image vertically — `texture(iChannel0, vec2(0.5, 0.0))` samples the **top** row of the image. Add `"flipY": true` to make `uv` match what you see in an image viewer, so a larger `uv.y` is closer to the top of the image:

```json
"0": { "type": "texture", "source": "photo.png", "flipY": true }
```

**Exported PNGs**: frames saved by `--images` are already flipped upright, matching the `uv` orientation in your shader (a larger `uv.y` is toward the top of the PNG), so no extra flip is needed.

**Export modes**: there is no input, so `iMouse` is always `vec4(0)` and the keyboard texture is all zeros.

### iMouse Format

- `iMouse.xy` = Mouse position while the button is down (updated as you drag, stays put after release)
- `abs(iMouse.zw)` = Mouse position during last click
- `sign(iMouse.z)` = Button is down (positive if down)
- `sign(iMouse.w)` = Just clicked (positive if clicked this frame)

### Keyboard Texture Format

The `type: "keyboard"` channel provides a 256x3 pixel texture:

- Row 0 (y=0): Current frame key state (keydown)
- Row 1 (y=1): Key just pressed event (keypressed)
- Row 2 (y=2): Key toggle state (toggles on each press)

```glsl
// Check if A key is pressed
float aPressed = texelFetch(iChannel0, ivec2(65, 0), 0).x;

// Check if Space was just pressed
float spaceJustPressed = texelFetch(iChannel0, ivec2(32, 1), 0).x;

// Check toggle state
float toggleState = texelFetch(iChannel0, ivec2(65, 2), 0).x;
```

Common key codes (JavaScript keyCode): A-Z (65-90), 0-9 (48-57), Space (32), Arrow keys (37-40), F1-F12 (112-123)

### Sound Shader

Sound passes generate audio in real-time. Define a `mainSound` function:

```glsl
vec2 mainSound(int samp, float time) {
    // Return vec2(leftChannel, rightChannel) in range -1.0 to 1.0
    float wave = sin(time * 440.0 * 6.28318) * 0.5;
    return vec2(wave, wave);
}
```

Sound shaders support `iSampleRate` (44100), `iSampleOffset`, and can read Buffer channels for audio-visual synchronization.

## Example Shaders

The `shaders/` directory contains several examples:

- `Rainforest/` - Rainforest effect
- `wormhole traversal/` - Wormhole traversal effect
- `blackhole/` - Black hole effect
- `Rainforest/` - Rainforest effect
- `EscapeTheGamegrid/` - Game effect

## Attribution

**All shaders in the `shaders/` directory are downloaded directly from [Shadertoy](https://www.shadertoy.com/) without any modification.**

To find the original author and source:
- Search by the **folder name** (e.g., "Rainforest", "wormhole traversal")
- Or search by the **`name` field** in the shader's `config.json`

This project only provides an emulator to run these shaders locally. All shader code belongs to their respective original authors on Shadertoy.

## Interaction

- **ESC** - Exit program
- **Mouse** - Drag interaction
- **Keyboard** - Read via keyboard channel

## License

GNU General Public License v3.0 (GPL-3.0)
