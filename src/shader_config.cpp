#include "shader_config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

std::optional<ChannelInput> ChannelInput::fromJson(const std::string& typeStr, const std::string& source,
                                                   const std::string& filterStr, const std::string& wrapStr,
                                                   bool flipY) {
    ChannelInput input;
    input.source = source;
    input.flipY = flipY;

    if (typeStr == "texture") {
        input.type = Type::Texture;
    } else if (typeStr == "buffer") {
        input.type = Type::Buffer;
    } else if (typeStr == "keyboard") {
        input.type = Type::Keyboard;
    } else {
        std::cerr << "Unknown channel type: " << typeStr << std::endl;
        return std::nullopt;
    }

    // filter/wrap 传空串表示配置里没写，缺省值按通道类型分：纹理跟着 Shadertoy 走
    // （mipmap + repeat）；buffer 和 keyboard 是数据不是图，插值会把相邻像素或键位
    // 糊在一起，越界也该夹住而不是平铺
    const bool isTexture = (input.type == Type::Texture);
    const Filter defaultFilter = isTexture ? Filter::Mipmap : Filter::Linear;
    const Wrap defaultWrap = isTexture ? Wrap::Repeat : Wrap::Clamp;

    // 解析 filter
    if (filterStr == "nearest") {
        input.filter = Filter::Nearest;
    } else if (filterStr == "mipmap") {
        input.filter = Filter::Mipmap;
    } else if (filterStr == "linear") {
        input.filter = Filter::Linear;
    } else {
        input.filter = defaultFilter;  // 没写，或者写了不认识的值
    }

    // 解析 wrap
    if (wrapStr == "repeat") {
        input.wrap = Wrap::Repeat;
    } else if (wrapStr == "mirror") {
        input.wrap = Wrap::Mirror;
    } else if (wrapStr == "clamp") {
        input.wrap = Wrap::Clamp;
    } else {
        input.wrap = defaultWrap;
    }

    return input;
}

bool ShaderConfig::load(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "Cannot open config file: " << jsonPath << std::endl;
        return false;
    }

    // 保存基准路径
    m_basePath = std::filesystem::path(jsonPath).parent_path();

    try {
        json config = json::parse(file);

        // 解析基本信息
        if (config.contains("name")) {
            m_name = config["name"].get<std::string>();
        }
        if (config.contains("width")) {
            m_width = config["width"].get<int>();
        }
        if (config.contains("height")) {
            m_height = config["height"].get<int>();
        }
        if (config.contains("resizable")) {
            m_resizable = config["resizable"].get<bool>();
        }
        if (config.contains("gui")) {
            m_gui = config["gui"].get<bool>();
        }
        if (config.contains("common")) {
            m_commonPath = config["common"].get<std::string>();
        }

        // 解析通道
        if (config.contains("passes")) {
            for (const auto& passJson : config["passes"]) {
                PassConfig pass;

                // 通道名称
                if (passJson.contains("name")) {
                    pass.name = passJson["name"].get<std::string>();
                } else {
                    std::cerr << "Pass missing 'name' field" << std::endl;
                    continue;
                }

                // Shader路径
                if (passJson.contains("shader")) {
                    pass.shaderPath = passJson["shader"].get<std::string>();
                } else {
                    std::cerr << "Pass '" << pass.name << "' missing 'shader' field" << std::endl;
                    continue;
                }

                // 可选的分辨率覆盖
                if (passJson.contains("width")) {
                    pass.width = passJson["width"].get<int>();
                }
                if (passJson.contains("height")) {
                    pass.height = passJson["height"].get<int>();
                }

                // 解析输入通道
                if (passJson.contains("channels")) {
                    for (auto& [key, value] : passJson["channels"].items()) {
                        // 键必须是 "0"-"3"。用 stoi 的话遇到别的键抛的异常看不出是哪个文件哪个通道
                        if (key.size() != 1 || key[0] < '0' || key[0] > '3') {
                            std::cerr << jsonPath << ": pass '" << pass.name
                                      << "': channel key must be \"0\"-\"3\", got \"" << key << "\""
                                      << std::endl;
                            continue;
                        }
                        const int channelIndex = key[0] - '0';

                        if (value.contains("type") && value.contains("source")) {
                            // 空串 = 没写，具体缺省由 fromJson 按通道类型决定
                            std::string filterStr = value.contains("filter") ? value["filter"].get<std::string>() : "";
                            std::string wrapStr = value.contains("wrap") ? value["wrap"].get<std::string>() : "";
                            bool flipY = value.contains("flipY") ? value["flipY"].get<bool>() : false;

                            auto input = ChannelInput::fromJson(
                                value["type"].get<std::string>(),
                                value["source"].get<std::string>(),
                                filterStr,
                                wrapStr,
                                flipY
                            );
                            if (input) {
                                pass.channels[channelIndex] = input;
                            }
                        }
                        // 兼容旧格式 "path" 和 "name"
                        else if (value.contains("type")) {
                            std::string typeStr = value["type"].get<std::string>();
                            // 旧格式：path/name 顶 source，键盘没有 source，也都没有
                            // filter/wrap——一样交给 fromJson 按类型取缺省
                            std::string source;
                            bool valid = true;
                            if (typeStr == "texture") {
                                valid = value.contains("path");
                                if (valid) source = value["path"].get<std::string>();
                            } else if (typeStr == "buffer") {
                                valid = value.contains("name");
                                if (valid) source = value["name"].get<std::string>();
                            } else if (typeStr != "keyboard") {
                                valid = false;
                            }
                            if (valid) {
                                auto input = ChannelInput::fromJson(typeStr, source, "", "", false);
                                if (input) {
                                    pass.channels[channelIndex] = input;
                                }
                            }
                        }
                    }
                }

                m_passes.push_back(std::move(pass));
            }
        }

    } catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }

    return !m_passes.empty();
}

ShaderConfig ShaderConfig::fromSingleShader(const std::string& shaderPath, int width, int height) {
    ShaderConfig config;
    config.m_name = "Single Shader";
    config.m_width = width;
    config.m_height = height;
    config.m_basePath = std::filesystem::path(shaderPath).parent_path();

    PassConfig imagePass;
    imagePass.name = "Image";
    imagePass.shaderPath = std::filesystem::path(shaderPath).filename().string();
    config.m_passes.push_back(std::move(imagePass));

    return config;
}
