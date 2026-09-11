#include "texture_cache.hpp"
#include <SFML/Graphics.hpp>
#include <glad/glad.h>
#include <iostream>
#include <vector>

TextureCache::~TextureCache() {
    for (GLuint s : m_samplers) {
        if (s) glDeleteSamplers(1, &s);
    }
}

std::string TextureCache::makeKey(const ChannelInput& input) {
    return input.source
        + (input.flipY ? ":flip" : "")
        + ":" + std::to_string(static_cast<int>(input.filter))
        + ":" + std::to_string(static_cast<int>(input.wrap));
}

GLTexture* TextureCache::get(const ChannelInput& input, const std::filesystem::path& basePath) {
    const std::string key = makeKey(input);
    auto it = m_textures.find(key);
    if (it != m_textures.end()) {
        return it->second.get();
    }

    const std::filesystem::path fullPath = basePath / input.source;

    sf::Image image;
    if (!image.loadFromFile(fullPath.string())) {
        std::cerr << "Failed to load texture: " << fullPath << std::endl;
        return nullptr;
    }
    if (input.flipY) {
        image.flipVertically();
    }

    auto texture = std::make_unique<GLTexture>();
    glGenTextures(1, &texture->id);
    glBindTexture(GL_TEXTURE_2D, texture->id);

    const sf::Vector2u size = image.getSize();
    texture->width = static_cast<int>(size.x);
    texture->height = static_cast<int>(size.y);

    // 统一存成浮点，和 Buffer 那边保持一致，shader 里采样不必区分来源
    std::vector<float> floatData(static_cast<size_t>(size.x) * size.y * 4);
    const uint8_t* pixelData = image.getPixelsPtr();
    for (size_t i = 0; i < floatData.size(); ++i) {
        floatData[i] = pixelData[i] / 255.0f;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size.x, size.y, 0, GL_RGBA, GL_FLOAT,
                 floatData.data());

    // 纹理自身的参数只是兜底，真正生效的是绑上来的 sampler object
    switch (input.filter) {
        case ChannelInput::Filter::Nearest:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case ChannelInput::Filter::Mipmap:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
            texture->mipmapDirty = false;  // 文件纹理内容不会再变，生成一次就够
            break;
        case ChannelInput::Filter::Linear:
        default:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }

    switch (input.wrap) {
        case ChannelInput::Wrap::Repeat:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            break;
        case ChannelInput::Wrap::Mirror:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
            break;
        case ChannelInput::Wrap::Clamp:
        default:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
    }

    m_textures[key] = std::move(texture);
    std::cout << "Loaded texture: " << fullPath << std::endl;
    return m_textures[key].get();
}

GLuint TextureCache::sampler(ChannelInput::Filter filter, ChannelInput::Wrap wrap) {
    const size_t index = static_cast<size_t>(filter) * 3 + static_cast<size_t>(wrap);
    if (m_samplers[index]) {
        return m_samplers[index];
    }

    GLuint s = 0;
    glGenSamplers(1, &s);

    switch (filter) {
        case ChannelInput::Filter::Nearest:
            glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case ChannelInput::Filter::Mipmap:
            glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        case ChannelInput::Filter::Linear:
        default:
            glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }

    GLenum wrapMode = GL_CLAMP_TO_EDGE;
    if (wrap == ChannelInput::Wrap::Repeat) {
        wrapMode = GL_REPEAT;
    } else if (wrap == ChannelInput::Wrap::Mirror) {
        wrapMode = GL_MIRRORED_REPEAT;
    }
    glSamplerParameteri(s, GL_TEXTURE_WRAP_S, wrapMode);
    glSamplerParameteri(s, GL_TEXTURE_WRAP_T, wrapMode);

    m_samplers[index] = s;
    return s;
}
