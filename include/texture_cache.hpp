#pragma once

#include "gl_framebuffer.hpp"
#include "pass_config.hpp"
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <string>

// 文件纹理和采样器的缓存。
// 采样参数挂在 sampler object 上而不是纹理自己身上：同一张图被不同通道以各自的
// filter/wrap 采样时互不干扰，也不必每帧去改纹理状态
class TextureCache {
public:
    ~TextureCache();

    // 取通道要用的纹理，没加载过就从文件读。读失败返回 nullptr
    GLTexture* get(const ChannelInput& input, const std::filesystem::path& basePath);

    // 取采样器。filter 和 wrap 各只有三种取值，直接查表
    GLuint sampler(ChannelInput::Filter filter, ChannelInput::Wrap wrap);

private:
    // 缓存 key 要带全配置：同一张图配不同 filter/wrap/flip 是不同条目
    static std::string makeKey(const ChannelInput& input);

    std::map<std::string, std::unique_ptr<GLTexture>> m_textures;
    std::array<GLuint, 9> m_samplers{};
};
