#include <iostream>
#include <string>
#include <filesystem>
#include <cxxopts.hpp>
#include "shader_config.hpp"
#include "shadertoy_emulator.hpp"
#include "glsl_preprocessor.hpp"
#include "run_options.hpp"

int main(int argc, char* argv[]) {
    cxxopts::Options options("ShadertoyEmulator", "Shadertoy Emulator - SFML 3");

    options.add_options()
        ("w,width", "Window width (overrides config)", cxxopts::value<int>()->default_value(std::to_string(ShaderConfig::DEFAULT_WIDTH)))
        ("h,height", "Window height (overrides config)", cxxopts::value<int>()->default_value(std::to_string(ShaderConfig::DEFAULT_HEIGHT)))
        ("show-fps", "Show FPS in console")
        ("fps", "Frame rate for exported images and video (default: 60)", cxxopts::value<int>()->default_value("60"))
        ("gui", "Enable GUI (overrides config)")
        ("no-gui", "Disable GUI (overrides config)")
        ("images", "Export a PNG sequence: frame range in Python slice syntax, e.g. 0:100:2 (stop required, excluded). Requires --output-dir", cxxopts::value<std::string>())
        ("output-dir", "Directory for the PNG sequence (required with --images)", cxxopts::value<std::string>())
        ("skip-intermediate", "Only render the frames selected by --images: the frames in between that nothing stateful depends on are skipped")
        ("force-skip-intermediate", "Skip the frames in between without checking dependencies. Faster, but wrong for shaders with cross-frame state")
        ("video", "Export a video to this path. Requires --duration", cxxopts::value<std::string>())
        ("duration", "Length of the exported video in seconds (required with --video)", cxxopts::value<int>())
        ("dump-audio", "Also write the Sound pass output to a WAV file", cxxopts::value<std::string>())
        ("debug-view", "Show this pass's buffer instead of the Image pass, e.g. BufferA", cxxopts::value<std::string>())
        ("dump-buffers", "Also write these buffer passes to <output-dir>/buffers/<name>/ (comma separated, or 'all')", cxxopts::value<std::string>())
        ("dump-buffer-gain", "Multiplier applied before clamping buffer values to 0..1 (default: 1)", cxxopts::value<float>()->default_value("1"))
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
            std::cout << "  With options:   ShadertoyEmulator config.json --width 1920 --height 1080 --show-fps\n";
            std::cout << "  Export images:  ShadertoyEmulator config.json --images 0:300:2 --output-dir frames/ --fps 30\n";
            std::cout << "  Export video:   ShadertoyEmulator config.json --video out.mp4 --duration 5 --fps 60\n";
            std::cout << "  Skip frames:    ShadertoyEmulator config.json --images 0:300:2 --output-dir frames/ --skip-intermediate\n";
            std::cout << "  Built-in prep:  ShadertoyEmulator config.json --builtin-preprocessor\n";
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
        bool useBuiltinPreprocessor = result.count("builtin-preprocessor") > 0;
        bool forceGui = result.count("gui") > 0;
        bool forceNoGui = result.count("no-gui") > 0;

        // 三种模式互斥：指定 --images 或 --video 就是导出模式，都没给就是窗口模式
        if (result.count("images") && result.count("video")) {
            std::cerr << "--images and --video are mutually exclusive.\n";
            return 1;
        }

        RunOptions runOptions;
        runOptions.showFps = result.count("show-fps") > 0;
        runOptions.skipIntermediate = result.count("skip-intermediate") > 0;
        runOptions.forceSkipIntermediate = result.count("force-skip-intermediate") > 0;
        if (runOptions.skipIntermediate && runOptions.forceSkipIntermediate) {
            std::cerr << "--skip-intermediate and --force-skip-intermediate are mutually exclusive.\n";
            return 1;
        }
        runOptions.fps = static_cast<float>(result["fps"].as<int>());
        if (runOptions.fps <= 0.0f) {
            std::cerr << "--fps must be positive.\n";
            return 1;
        }
        if (result.count("dump-audio") > 0) {
            runOptions.audioDumpPath = result["dump-audio"].as<std::string>();
        }
        if (result.count("debug-view") > 0) {
            runOptions.debugViewPass = result["debug-view"].as<std::string>();
        }
        if (result.count("dump-buffers") > 0) {
            // 逗号分隔的名字列表。允许写成 "BufferA, BufferC"，顺手把空白去掉
            const std::string names = result["dump-buffers"].as<std::string>();
            size_t pos = 0;
            while (pos < names.size()) {
                const size_t comma = names.find(',', pos);
                const std::string item = names.substr(
                    pos, comma == std::string::npos ? std::string::npos : comma - pos);

                const size_t begin = item.find_first_not_of(" \t");
                if (begin != std::string::npos) {
                    runOptions.dumpBuffers.push_back(
                        item.substr(begin, item.find_last_not_of(" \t") - begin + 1));
                }
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            runOptions.dumpBufferGain = result["dump-buffer-gain"].as<float>();
        }

        // 视频模式不导调试帧：调试看的是中间结果，图片序列已经够用了
        if (result.count("dump-buffers") > 0 && result.count("images") == 0) {
            std::cerr << "--dump-buffers only applies to --images mode.\n";
            return 1;
        }

        // 视频每一帧都得有，窗口模式也不导帧，跳帧只对图片序列有意义
        if ((result.count("skip-intermediate") > 0 || result.count("force-skip-intermediate") > 0)
            && result.count("images") == 0) {
            std::cerr << "--skip-intermediate/--force-skip-intermediate only apply to --images mode.\n";
            return 1;
        }

        // 参数用错模式时报错而不是默默忽略，否则很容易以为生效了
        if (result.count("images")) {
            runOptions.mode = RunMode::Images;

            std::string imagesText = result["images"].as<std::string>();
            if (!parseFrameRange(imagesText, runOptions.imageRange)) {
                std::cerr << "Invalid --images value: '" << imagesText << "'\n"
                          << "Expected Python slice syntax like 0:100:2 (stop is required and excluded).\n";
                return 1;
            }
            if (result.count("duration") > 0) {
                std::cerr << "--duration only applies to --video mode.\n";
                return 1;
            }
            if (result.count("output-dir") == 0) {
                std::cerr << "--images requires --output-dir.\n";
                return 1;
            }
            runOptions.imageDir = result["output-dir"].as<std::string>();
        } else if (result.count("video")) {
            runOptions.mode = RunMode::Video;
            runOptions.videoPath = result["video"].as<std::string>();

            if (result.count("output-dir") > 0) {
                std::cerr << "--output-dir only applies to --images mode; --video already carries the path.\n";
                return 1;
            }
            if (result.count("duration") == 0 || result["duration"].as<int>() <= 0) {
                std::cerr << "--video requires a positive --duration (seconds).\n";
                return 1;
            }
            runOptions.videoSeconds = result["duration"].as<int>();
        } else {
            if (result.count("output-dir") > 0) {
                std::cerr << "--output-dir only applies to --images mode.\n";
                return 1;
            }
            if (result.count("duration") > 0) {
                std::cerr << "--duration only applies to --video mode.\n";
                return 1;
            }
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

        // GUI 设置：命令行参数优先于配置文件，导出模式一律关闭
        if (forceGui) {
            runOptions.enableGui = true;
        } else if (forceNoGui) {
            runOptions.enableGui = false;
        } else {
            runOptions.enableGui = isJson && config.hasGui();
        }
        if (runOptions.isOffscreen()) {
            runOptions.enableGui = false;
        }

        ShadertoyEmulator emulator(config, runOptions);

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
