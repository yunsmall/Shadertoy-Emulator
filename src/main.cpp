#include <iostream>
#include <string>
#include <filesystem>
#include <cxxopts.hpp>
#include "shader_config.hpp"
#include "shadertoy_emulator.hpp"
#include "glsl_preprocessor.hpp"
#include "frame_range.hpp"

int main(int argc, char* argv[]) {
    cxxopts::Options options("ShadertoyEmulator", "Shadertoy Emulator - SFML 3");

    options.add_options()
        ("w,width", "Window width (overrides config)", cxxopts::value<int>()->default_value(std::to_string(ShaderConfig::DEFAULT_WIDTH)))
        ("h,height", "Window height (overrides config)", cxxopts::value<int>()->default_value(std::to_string(ShaderConfig::DEFAULT_HEIGHT)))
        ("show-fps", "Show FPS in console")
        ("fps", "Virtual frame rate for offscreen rendering (default: 60)", cxxopts::value<int>()->default_value("60"))
        ("gui", "Enable GUI (overrides config)")
        ("no-gui", "Disable GUI (overrides config)")
        ("frames", "Frame range to save as PNG, Python slice syntax, e.g. 0:100:2 (stop required, excluded)", cxxopts::value<std::string>())
        ("output-dir", "Directory for saved frames (default: current directory)", cxxopts::value<std::string>()->default_value("."))
        ("offscreen", "Offscreen rendering: no window, no ImGui, no audio, no input (requires --frames)")
        ("builtin-preprocessor", "Use built-in GLSL preprocessor instead of external (glslangValidator)")
        ("input", "Shader file or config.json path (positional)", cxxopts::value<std::string>())
        ("help", "Print usage");

    options.parse_positional({"input"});
    options.positional_help("<shader.glsl|config.json>");

    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            std::cout << "\nExamples:\n";
            std::cout << "  Single shader:  ShadertoyEmulator shader.glsl\n";
            std::cout << "  Multi-pass:     ShadertoyEmulator config.json\n";
            std::cout << "  With options:   ShadertoyEmulator config.json --width 1920 --height 1080 --fps\n";
            std::cout << "  Built-in prep:  ShadertoyEmulator config.json --builtin-preprocessor\n";
            std::cout << "  Export frames:  ShadertoyEmulator config.json --offscreen --frames 0:300:2 --output-dir frames/ --fps 30\n";
            return 0;
        }

        if (!result.count("input")) {
            std::cerr << "Error: No shader file or config specified.\n\n";
            std::cout << options.help() << std::endl;
            return 1;
        }

        std::string inputPath = result["input"].as<std::string>();
        int width = result["width"].as<int>();
        int height = result["height"].as<int>();
        bool showFps = result.count("show-fps") > 0;

        int offlineFps = result["fps"].as<int>();
        if (offlineFps <= 0) {
            std::cerr << "--fps must be positive.\n";
            return 1;
        }
        bool useBuiltinPreprocessor = result.count("builtin-preprocessor") > 0;
        bool forceGui = result.count("gui") > 0;
        bool forceNoGui = result.count("no-gui") > 0;
        bool offscreen = result.count("offscreen") > 0;

        // 帧序列导出参数
        bool captureEnabled = result.count("frames") > 0;
        FrameRange captureRange;
        if (captureEnabled) {
            std::string framesText = result["frames"].as<std::string>();
            if (!parseFrameRange(framesText, captureRange)) {
                std::cerr << "Invalid --frames value: '" << framesText << "'\n"
                          << "Expected Python slice syntax like 0:100:2 (stop is required and excluded).\n";
                return 1;
            }
        }
        std::filesystem::path captureDir = result["output-dir"].as<std::string>();

        if (offscreen && !captureEnabled) {
            std::cerr << "--offscreen requires --frames, otherwise there is no exit condition.\n";
            return 1;
        }

        // 设置预处理器模式
        GlslPreprocessor::Mode preprocessorMode = useBuiltinPreprocessor
            ? GlslPreprocessor::Mode::BuiltIn
            : GlslPreprocessor::Mode::External;
        GlslPreprocessor::setDefaultMode(preprocessorMode);

        // 判断是JSON配置还是单个shader文件
        ShaderConfig config;
        bool isJson = std::filesystem::path(inputPath).extension() == ".json";

        if (isJson) {
            std::cout << "Loading config: " << inputPath << "\n";
            if (!config.load(inputPath)) {
                std::cerr << "Failed to load config file.\n";
                return 1;
            }
        } else {
            // 单shader模式
            std::cout << "Loading single shader: " << inputPath << "\n";
            config = ShaderConfig::fromSingleShader(inputPath, width, height);
        }

        // 命令行参数覆盖配置文件中的分辨率
        if (result.count("width")) {
            config.setWidth(width);
        }
        if (result.count("height")) {
            config.setHeight(height);
        }

        std::cout << "Creating Shadertoy Emulator...\n";
        std::cout << "Window: " << config.getWidth() << "x" << config.getHeight() << "\n";
        std::cout << "Passes: " << config.getPasses().size() << "\n";

        // GUI 设置：命令行参数优先于配置文件，离屏模式一律关闭
        bool enableGui;
        if (forceGui) {
            enableGui = true;
        } else if (forceNoGui) {
            enableGui = false;
        } else {
            enableGui = isJson && config.hasGui();
        }
        if (offscreen) {
            enableGui = false;
        }
        ShadertoyEmulator emulator(config, showFps, enableGui, offscreen, captureRange, captureDir,
                                   static_cast<float>(offlineFps));

        std::cout << "Running... Press ESC to exit.\n";
        emulator.run();

    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        std::cout << options.help() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
