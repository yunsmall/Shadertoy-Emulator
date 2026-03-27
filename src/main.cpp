#include <iostream>
#include <string>
#include <filesystem>
#include <cxxopts.hpp>
#include "shader_config.hpp"
#include "shadertoy_emulator.hpp"
#include "glsl_preprocessor.hpp"

int main(int argc, char* argv[]) {
    cxxopts::Options options("shadertoy_test", "Shadertoy Emulator - SFML 3");

    options.add_options()
        ("w,width", "Window width (overrides config)", cxxopts::value<int>()->default_value(std::to_string(ShaderConfig::DEFAULT_WIDTH)))
        ("h,height", "Window height (overrides config)", cxxopts::value<int>()->default_value(std::to_string(ShaderConfig::DEFAULT_HEIGHT)))
        ("fps", "Show FPS in console")
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
            std::cout << "  Single shader:  shadertoy_test shader.glsl\n";
            std::cout << "  Multi-pass:     shadertoy_test config.json\n";
            std::cout << "  With options:   shadertoy_test config.json --width 1920 --height 1080 --fps\n";
            std::cout << "  Built-in prep:  shadertoy_test config.json --builtin-preprocessor\n";
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
        bool showFps = result.count("fps") > 0;
        bool useBuiltinPreprocessor = result.count("builtin-preprocessor") > 0;

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

        ShadertoyEmulator emulator(config, showFps);

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
