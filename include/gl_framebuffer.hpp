#pragma once

#include <glad/glad.h>
#include <utility>

// OpenGL 浮点纹理封装
class GLTexture {
public:
    GLuint id = 0;
    int width = 1;
    int height = 1;
    bool mipmapDirty = true;  // 内容变了就得重新生成 mipmap，默认视为待生成

    GLTexture() = default;
    GLTexture(const GLTexture&) = delete;
    GLTexture& operator=(const GLTexture&) = delete;

    GLTexture(GLTexture&& other) noexcept
        : id(other.id), width(other.width), height(other.height), mipmapDirty(other.mipmapDirty) {
        other.id = 0;
    }

    GLTexture& operator=(GLTexture&& other) noexcept {
        if (this != &other) {
            destroy();
            id = other.id;
            width = other.width;
            height = other.height;
            mipmapDirty = other.mipmapDirty;
            other.id = 0;
        }
        return *this;
    }

    ~GLTexture() { destroy(); }

    void bind(int unit = 0) const {
        if (id) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, id);
        }
    }

    void destroy() {
        if (id) {
            glDeleteTextures(1, &id);
            id = 0;
        }
    }
};

// OpenGL 浮点 FBO 封装
class GLFramebuffer {
public:
    GLuint fbo = 0;
    GLTexture colorTex;

    GLFramebuffer() = default;
    GLFramebuffer(const GLFramebuffer&) = delete;
    GLFramebuffer& operator=(const GLFramebuffer&) = delete;

    GLFramebuffer(GLFramebuffer&& other) noexcept
        : fbo(other.fbo), colorTex(std::move(other.colorTex)) {
        other.fbo = 0;
    }

    GLFramebuffer& operator=(GLFramebuffer&& other) noexcept {
        if (this != &other) {
            destroy();
            fbo = other.fbo;
            colorTex = std::move(other.colorTex);
            other.fbo = 0;
        }
        return *this;
    }

    ~GLFramebuffer() { destroy(); }

    bool create(int w, int h, GLenum internalFormat = GL_RGBA32F) {
        if (w <= 0 || h <= 0) return false;

        destroy();

        // 创建 FBO
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // 创建颜色纹理。默认浮点，让 Buffer 之间能传 HDR 中间结果；
        // 离屏导出用 GL_RGBA8，这样 glReadPixels 能直接读 GL_UNSIGNED_BYTE
        glGenTextures(1, &colorTex.id);
        glBindTexture(GL_TEXTURE_2D, colorTex.id);

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        colorTex.width = w;
        colorTex.height = h;

        // 附加到 FBO
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex.id, 0);

        bool complete = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        if (complete) {
            // 清屏
            const GLfloat clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
            glClearBufferfv(GL_COLOR, 0, clearColor);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return complete;
    }

    void bind() const {
        if (fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        }
    }

    static void unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void clear(float r = 0.f, float g = 0.f, float b = 0.f, float a = 0.f) {
        const GLfloat clearColor[4] = { r, g, b, a };
        glClearBufferfv(GL_COLOR, 0, clearColor);
    }

    void destroy() {
        colorTex.destroy();
        if (fbo) {
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
    }
};
