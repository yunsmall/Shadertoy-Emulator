#pragma once

#include <string>
#include <set>
#include <filesystem>

class GlslPreprocessor {
public:
    enum class Mode { BuiltIn, External };

    GlslPreprocessor() : m_mode(s_defaultMode) {}

    // 设置全局默认模式
    static void setDefaultMode(Mode mode) { s_defaultMode = mode; }
    static Mode getDefaultMode() { return s_defaultMode; }

    // 预处理GLSL代码。要共享宏的代码得先拼成一段再传进来——外部预处理器
    // （glslangValidator）每次都是独立跑完整套，没有"接着上次的宏"这回事
    std::string process(const std::string& code,
                        const std::filesystem::path& basePath,
                        int maxIncludeDepth = 10);

    // 清除状态（用于处理新文件时重置）
    void reset();

    void setMode(Mode mode) { m_mode = mode; }
    Mode getMode() const { return m_mode; }

private:
    Mode m_mode;
    static Mode s_defaultMode;

    // m_mode 为 External 只代表用户的意图，PATH 里没有 glslangValidator 时还得退回内置
    bool shouldUseExternal() const;
    static bool externalValidatorAvailable();

    // ========== 内置预处理器 ==========
    // 只干一件事：#include。其余指令（#define / #if / #ifdef / #undef / #pragma…）一律
    // 原样透传给 GLSL 编译器——它们本来就是 GLSL 语言的一部分，驱动自带的那套完整得多，
    // 自己去实现 #if 的表达式求值只会慢一步而且更容易错
    std::set<std::filesystem::path> m_includedFiles;  // 防止循环引用

    // 主处理。currentDir 始终是"当前文件所在目录"，#include 的相对路径基于它解析
    std::string processCode(const std::string& code,
                            const std::filesystem::path& currentDir,
                            int depth);

    std::string processInclude(const std::string& args,
                               const std::filesystem::path& currentDir,
                               int depth);

    static std::string trim(const std::string& str);

    // ========== 外部预处理器 ==========
    static std::string runExternalPreprocessor(const std::string& code,
                                                const std::filesystem::path& basePath);
};
