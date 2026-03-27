#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>
#include <filesystem>

class GlslPreprocessor {
public:
    enum class Mode { BuiltIn, External };

    GlslPreprocessor() : m_mode(s_defaultMode) {}

    // 设置全局默认模式
    static void setDefaultMode(Mode mode) { s_defaultMode = mode; }
    static Mode getDefaultMode() { return s_defaultMode; }

    // 预处理文件（外部预处理器直接处理文件）
    std::string processFile(const std::filesystem::path& filePath);

    // 预处理GLSL代码（清空状态后处理）
    std::string process(const std::string& code,
                        const std::filesystem::path& basePath,
                        int maxIncludeDepth = 10);

    // 继续处理（保留宏定义，用于处理后续代码）
    std::string continueProcess(const std::string& code,
                                const std::filesystem::path& basePath,
                                int maxIncludeDepth = 10);

    // 清除状态（用于处理新文件时重置）
    void reset();

    void setMode(Mode mode) { m_mode = mode; }
    Mode getMode() const { return m_mode; }

private:
    Mode m_mode;
    static Mode s_defaultMode;

    // ========== 内置预处理器 ==========
    // 宏定义
    struct Macro {
        std::string name;
        std::vector<std::string> params;  // 空 = 无参数宏
        std::string body;
    };

    std::map<std::string, Macro> m_macros;
    std::set<std::filesystem::path> m_includedFiles;  // 防止循环引用
    std::vector<bool> m_conditionStack;  // 条件编译栈（true = 当前分支激活）

    // 主处理
    std::string processCode(const std::string& code,
                            const std::filesystem::path& currentPath,
                            int depth);

    // 指令处理
    std::string processInclude(const std::string& args,
                               const std::filesystem::path& currentPath,
                               int depth);
    void processDefine(const std::string& args);
    void processUndef(const std::string& args);
    bool processIfdef(const std::string& args, bool isIfndef = false);
    void processElse();
    void processEndif();

    // 宏展开
    std::string expandMacros(const std::string& text);

    // 条件状态
    bool isActive() const;  // 当前是否在激活的代码块中

    // 辅助函数
    static std::string trim(const std::string& str);
    static std::string stripComments(const std::string& code);
    static std::vector<std::string> tokenize(const std::string& str, const std::string& delims);
    static std::string extractString(const std::string& str, size_t start);

    // ========== 外部预处理器 ==========
    static std::string runExternalPreprocessor(const std::string& code,
                                                const std::filesystem::path& basePath);
};