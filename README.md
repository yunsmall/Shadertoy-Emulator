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
- glslangValidator (optional, for external preprocessor)

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

### Run a single shader

```bash
shadertoy_test shader.glsl
```

### Run with JSON configuration

```bash
shadertoy_test config.json
```

### Command Line Arguments

| Argument | Description |
|----------|-------------|
| `--width <n>` | Override window width |
| `--height <n>` | Override window height |
| `--fps` | Display frame rate in console |
| `--builtin-preprocessor` | Use built-in GLSL preprocessor |

## Configuration

Create a `config.json` to configure multi-pass shaders:

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

For detailed configuration, see [shaders/JSON_CONFIG.md](shaders/JSON_CONFIG.md).

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

### iMouse Format

- `iMouse.xy` = Mouse position during last button down
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
