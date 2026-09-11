#pragma once

#include <string>
#include <array>
#include <optional>

// 输入通道配置
struct ChannelInput {
    enum class Type { Texture, Buffer, Keyboard };
    enum class Filter { Linear, Nearest, Mipmap };
    enum class Wrap { Clamp, Repeat, Mirror };

    Type type;
    std::string source;  // 文件路径或Buffer名称
    // 这两个只是内存初值，不是配置缺省值——缺省按通道类型分，在 fromJson 里定
    // （纹理 mipmap + repeat，buffer/keyboard linear + clamp）
    Filter filter = Filter::Linear;
    Wrap wrap = Wrap::Clamp;
    bool flipY = false;  // 是否上下翻转

    // JSON反序列化。filterStr/wrapStr 传空串表示配置里没写，按 type 取缺省
    static std::optional<ChannelInput> fromJson(const std::string& typeStr, const std::string& source,
                                                  const std::string& filterStr, const std::string& wrapStr,
                                                  bool flipY = false);
};

// 渲染通道配置
struct PassConfig {
    std::string name;                                       // BufferA/B/C/D 或 Image
    std::string shaderPath;                                 // shader文件路径
    int width = 0;                                          // 0表示使用窗口分辨率
    int height = 0;
    std::array<std::optional<ChannelInput>, 4> channels;   // iChannel0-3

    bool isImage() const { return name == "Image"; }
    bool isSound() const { return name == "Sound"; }
    bool isBuffer() const { return !isImage() && !isSound(); }
};
