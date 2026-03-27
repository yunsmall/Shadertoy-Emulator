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

    // 解析 filter
    if (filterStr == "nearest") {
        input.filter = Filter::Nearest;
    } else if (filterStr == "mipmap") {
        input.filter = Filter::Mipmap;
    } else {
        input.filter = Filter::Linear;  // 默认
    }

    // 解析 wrap
    if (wrapStr == "repeat") {
        input.wrap = Wrap::Repeat;
    } else if (wrapStr == "mirror") {
        input.wrap = Wrap::Mirror;
    } else {
        input.wrap = Wrap::Clamp;  // 默认
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
                        int channelIndex = std::stoi(key);
                        if (channelIndex < 0 || channelIndex > 3) {
                            std::cerr << "Invalid channel index: " << key << std::endl;
                            continue;
                        }

                        if (value.contains("type") && value.contains("source")) {
                            std::string filterStr = value.contains("filter") ? value["filter"].get<std::string>() : "linear";
                            std::string wrapStr = value.contains("wrap") ? value["wrap"].get<std::string>() : "clamp";
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
                            if (typeStr == "texture" && value.contains("path")) {
                                ChannelInput input;
                                input.type = ChannelInput::Type::Texture;
                                input.source = value["path"].get<std::string>();
                                pass.channels[channelIndex] = input;
                            } else if (typeStr == "buffer" && value.contains("name")) {
                                ChannelInput input;
                                input.type = ChannelInput::Type::Buffer;
                                input.source = value["name"].get<std::string>();
                                pass.channels[channelIndex] = input;
                            } else if (typeStr == "keyboard") {
                                ChannelInput input;
                                input.type = ChannelInput::Type::Keyboard;
                                pass.channels[channelIndex] = input;
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
