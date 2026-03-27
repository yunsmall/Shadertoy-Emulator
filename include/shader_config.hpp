#pragma once

#include "pass_config.hpp"
#include <string>
#include <vector>
#include <filesystem>

class ShaderConfig {
public:
    // 默认分辨率
    static constexpr int DEFAULT_WIDTH = 1280;
    static constexpr int DEFAULT_HEIGHT = 720;

    // 从JSON文件加载配置
    bool load(const std::string& jsonPath);

    // 获取配置信息
    const std::string& getName() const { return m_name; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool isResizable() const { return m_resizable; }
    const std::string& getCommonPath() const { return m_commonPath; }
    const std::vector<PassConfig>& getPasses() const { return m_passes; }
    const std::filesystem::path& getBasePath() const { return m_basePath; }

    // 设置覆盖
    void setWidth(int width) { m_width = width; }
    void setHeight(int height) { m_height = height; }
    void setResizable(bool resizable) { m_resizable = resizable; }

    // 解析单通道模式（兼容旧版）
    static ShaderConfig fromSingleShader(const std::string& shaderPath, int width, int height);

private:
    std::string m_name;
    int m_width = DEFAULT_WIDTH;
    int m_height = DEFAULT_HEIGHT;
    bool m_resizable = false;  // 默认不可调整大小
    std::string m_commonPath;  // 相对路径
    std::vector<PassConfig> m_passes;
    std::filesystem::path m_basePath;  // JSON文件所在目录，用于解析相对路径
};
