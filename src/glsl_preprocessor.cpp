#include "glsl_preprocessor.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

// 静态成员初始化 - 默认使用外部预处理器
GlslPreprocessor::Mode GlslPreprocessor::s_defaultMode = GlslPreprocessor::Mode::External;

void GlslPreprocessor::reset() {
    m_macros.clear();
    m_includedFiles.clear();
    m_conditionStack.clear();
}

std::string GlslPreprocessor::process(const std::string& code,
                                       const std::filesystem::path& basePath,
                                       int maxIncludeDepth) {
    if (m_mode == Mode::External) {
        return runExternalPreprocessor(code, basePath);
    }
    reset();
    return processCode(code, basePath, maxIncludeDepth);
}

std::string GlslPreprocessor::continueProcess(const std::string& code,
                                               const std::filesystem::path& basePath,
                                               int maxIncludeDepth) {
    if (m_mode == Mode::External) {
        return runExternalPreprocessor(code, basePath);
    }
    // 不清空宏定义，保留之前定义的宏
    m_includedFiles.clear();  // 但清除包含记录，允许处理新文件
    m_conditionStack.clear();
    return processCode(code, basePath, maxIncludeDepth);
}

// ========== 外部预处理器实现 ==========

std::string GlslPreprocessor::processFile(const std::filesystem::path& filePath) {
    // 读取文件内容
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "GLSL Preprocessor: cannot open file: " << filePath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    if (m_mode == Mode::External) {
        return runExternalPreprocessor(buffer.str(), filePath.parent_path());
    }
    return processCode(buffer.str(), filePath, 10);
}

std::string GlslPreprocessor::runExternalPreprocessor(const std::string& code,
                                                       const std::filesystem::path& basePath) {
    // 创建临时文件（放在 basePath 目录下，这样 #include 相对路径能正确工作）
    std::filesystem::path tempFile = basePath / ".glsl_preprocess_temp.frag";

    // 写入代码
    {
        std::ofstream out(tempFile, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "GLSL Preprocessor: cannot create temp file" << std::endl;
            return code;
        }
        out << code;
    }

    // 构建 glslangValidator 命令
    // -S frag 指定为 fragment shader
    // -E 只做预处理
    std::string cmd = "glslangValidator -S frag -E \"" + tempFile.string() + "\"";

    // 执行命令并捕获输出
    std::string output;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif

    if (!pipe) {
        std::cerr << "GLSL Preprocessor: failed to run glslangValidator" << std::endl;
        std::filesystem::remove(tempFile);
        return code;
    }

    // 动态读取输出
    std::vector<char> buffer(1024);
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output += buffer.data();
        // 如果缓冲区不够大，动态扩展
        if (strlen(buffer.data()) == buffer.size() - 1 && buffer.back() != '\n') {
            buffer.resize(buffer.size() * 2);
        }
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    // 删除临时文件
    std::filesystem::remove(tempFile);

    return output;
}

// ========== 内置预处理器实现 ==========

std::string GlslPreprocessor::processCode(const std::string& code,
                                           const std::filesystem::path& currentPath,
                                           int depth) {
    if (depth < 0) {
        std::cerr << "GLSL Preprocessor: max include depth exceeded" << std::endl;
        return "";
    }

    std::istringstream input(code);
    std::ostringstream output;
    std::string line;

    while (std::getline(input, line)) {
        // 去除行尾空白
        std::string trimmedLine = trim(line);

        // 处理预处理指令
        if (!trimmedLine.empty() && trimmedLine[0] == '#') {
            // 解析指令
            size_t spacePos = trimmedLine.find_first_of(" \t");
            std::string directive = (spacePos != std::string::npos)
                                    ? trimmedLine.substr(0, spacePos)
                                    : trimmedLine;
            std::string args = (spacePos != std::string::npos)
                               ? trim(trimmedLine.substr(spacePos + 1))
                               : "";

            if (directive == "#include") {
                if (isActive()) {
                    std::string included = processInclude(args, currentPath, depth - 1);
                    output << included;
                }
            } else if (directive == "#define") {
                if (isActive()) {
                    processDefine(args);
                }
            } else if (directive == "#undef") {
                if (isActive()) {
                    processUndef(args);
                }
            } else if (directive == "#ifdef") {
                processIfdef(args, false);
            } else if (directive == "#ifndef") {
                processIfdef(args, true);
            } else if (directive == "#else") {
                processElse();
            } else if (directive == "#endif") {
                processEndif();
            } else if (directive == "#pragma") {
                // 直接传递 #pragma 指令（如 #pragma once）
                if (isActive()) {
                    output << line << "\n";
                }
            } else {
                // 其他预处理指令直接传递（如 #version）
                if (isActive()) {
                    output << line << "\n";
                }
            }
        } else {
            // 普通代码行
            if (isActive()) {
                // 宏展开
                std::string expanded = expandMacros(line);
                output << expanded << "\n";
            }
        }
    }

    // 检查条件栈是否平衡
    if (!m_conditionStack.empty()) {
        std::cerr << "GLSL Preprocessor: unbalanced #if/#endif" << std::endl;
    }

    return output.str();
}

std::string GlslPreprocessor::processInclude(const std::string& args,
                                              const std::filesystem::path& currentPath,
                                              int depth) {
    // 提取文件路径（支持 "path" 和 <path>）
    if (args.empty() || (args[0] != '"' && args[0] != '<')) {
        std::cerr << "GLSL Preprocessor: invalid #include syntax: " << args << std::endl;
        return "";
    }

    char endChar = (args[0] == '"') ? '"' : '>';
    size_t endPos = args.find(endChar, 1);
    if (endPos == std::string::npos) {
        std::cerr << "GLSL Preprocessor: unterminated #include path: " << args << std::endl;
        return "";
    }

    std::string includePath = args.substr(1, endPos - 1);

    // 解析完整路径
    std::filesystem::path fullPath = currentPath.parent_path() / includePath;
    fullPath = std::filesystem::weakly_canonical(fullPath);

    // 检查循环引用
    if (m_includedFiles.count(fullPath)) {
        // 文件已包含，跳过
        return "";
    }
    m_includedFiles.insert(fullPath);

    // 读取文件
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "GLSL Preprocessor: cannot open include file: " << fullPath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    // 递归处理
    return processCode(buffer.str(), fullPath, depth);
}

void GlslPreprocessor::processDefine(const std::string& args) {
    if (args.empty()) {
        std::cerr << "GLSL Preprocessor: empty #define" << std::endl;
        return;
    }

    // 解析宏名
    size_t nameEnd = args.find_first_of(" \t(");
    std::string name = args.substr(0, nameEnd);

    if (nameEnd == std::string::npos) {
        // 无参数、无值的宏
        m_macros[name] = Macro{name, {}, ""};
        return;
    }

    std::string rest = trim(args.substr(nameEnd));

    // 检查是否是函数宏
    if (rest[0] == '(') {
        // 解析参数列表
        size_t closeParen = rest.find(')');
        if (closeParen == std::string::npos) {
            std::cerr << "GLSL Preprocessor: missing ')' in #define " << name << std::endl;
            return;
        }

        std::string paramsStr = rest.substr(1, closeParen - 1);
        std::string body = trim(rest.substr(closeParen + 1));

        // 解析参数
        std::vector<std::string> params;
        if (!paramsStr.empty()) {
            std::istringstream ps(paramsStr);
            std::string param;
            while (std::getline(ps, param, ',')) {
                params.push_back(trim(param));
            }
        }

        m_macros[name] = Macro{name, params, body};
    } else {
        // 简单宏
        m_macros[name] = Macro{name, {}, rest};
    }
}

void GlslPreprocessor::processUndef(const std::string& args) {
    std::string name = trim(args);
    m_macros.erase(name);
}

bool GlslPreprocessor::processIfdef(const std::string& args, bool isIfndef) {
    std::string name = trim(args);
    bool defined = m_macros.count(name) > 0;

    if (m_conditionStack.empty()) {
        // 顶层条件
        m_conditionStack.push_back(defined != isIfndef);
    } else {
        // 嵌套条件：只有外层都激活时才考虑当前条件
        bool parentActive = isActive();
        m_conditionStack.push_back(parentActive && (defined != isIfndef));
    }

    // 返回当前条件状态（用于 #else）
    return m_conditionStack.back();
}

void GlslPreprocessor::processElse() {
    if (m_conditionStack.empty()) {
        std::cerr << "GLSL Preprocessor: #else without #if" << std::endl;
        return;
    }

    // 切换当前条件
    m_conditionStack.back() = !m_conditionStack.back();
}

void GlslPreprocessor::processEndif() {
    if (m_conditionStack.empty()) {
        std::cerr << "GLSL Preprocessor: #endif without #if" << std::endl;
        return;
    }

    m_conditionStack.pop_back();
}

bool GlslPreprocessor::isActive() const {
    // 所有条件都为 true 时才激活
    for (bool active : m_conditionStack) {
        if (!active) return false;
    }
    return true;
}

std::string GlslPreprocessor::expandMacros(const std::string& text) {
    std::string result = text;
    bool changed = true;
    int iterations = 0;
    const int maxIterations = 100;  // 防止无限循环

    while (changed && iterations < maxIterations) {
        changed = false;
        iterations++;

        for (const auto& [name, macro] : m_macros) {
            if (macro.params.empty()) {
                // 简单宏替换
                size_t pos = 0;
                while ((pos = result.find(name, pos)) != std::string::npos) {
                    // 检查是否是标识符的一部分
                    bool validStart = (pos == 0 || !std::isalnum(result[pos - 1]) && result[pos - 1] != '_');
                    bool validEnd = (pos + name.length() >= result.length() ||
                                    (!std::isalnum(result[pos + name.length()]) && result[pos + name.length()] != '_'));

                    if (validStart && validEnd) {
                        result.replace(pos, name.length(), macro.body);
                        pos += macro.body.length();
                        changed = true;
                    } else {
                        pos++;
                    }
                }
            } else {
                // 带参数的宏
                size_t pos = 0;
                while ((pos = result.find(name, pos)) != std::string::npos) {
                    bool validStart = (pos == 0 || !std::isalnum(result[pos - 1]) && result[pos - 1] != '_');
                    bool validEnd = (pos + name.length() >= result.length() ||
                                    !std::isalnum(result[pos + name.length()]) && result[pos + name.length()] != '_');

                    if (validStart && validEnd) {
                        // 查找参数列表
                        size_t parenPos = result.find('(', pos + name.length());
                        if (parenPos != std::string::npos && parenPos == pos + name.length()) {
                            // 找到匹配的右括号
                            int parenCount = 1;
                            size_t endPos = parenPos + 1;
                            while (endPos < result.length() && parenCount > 0) {
                                if (result[endPos] == '(') parenCount++;
                                else if (result[endPos] == ')') parenCount--;
                                endPos++;
                            }

                            if (parenCount == 0) {
                                // 提取参数
                                std::string argsStr = result.substr(parenPos + 1, endPos - parenPos - 2);
                                std::vector<std::string> args;
                                if (!argsStr.empty()) {
                                    int argParenCount = 0;
                                    size_t argStart = 0;
                                    for (size_t i = 0; i <= argsStr.length(); ++i) {
                                        if (i == argsStr.length() || (argsStr[i] == ',' && argParenCount == 0)) {
                                            args.push_back(trim(argsStr.substr(argStart, i - argStart)));
                                            argStart = i + 1;
                                        } else if (argsStr[i] == '(') {
                                            argParenCount++;
                                        } else if (argsStr[i] == ')') {
                                            argParenCount--;
                                        }
                                    }
                                }

                                // 替换参数
                                std::string body = macro.body;
                                for (size_t i = 0; i < macro.params.size() && i < args.size(); ++i) {
                                    size_t paramPos = 0;
                                    while ((paramPos = body.find(macro.params[i], paramPos)) != std::string::npos) {
                                        bool validPStart = (paramPos == 0 || !std::isalnum(body[paramPos - 1]) && body[paramPos - 1] != '_');
                                        bool validPEnd = (paramPos + macro.params[i].length() >= body.length() ||
                                                        !std::isalnum(body[paramPos + macro.params[i].length()]) && body[paramPos + macro.params[i].length()] != '_');

                                        if (validPStart && validPEnd) {
                                            body.replace(paramPos, macro.params[i].length(), args[i]);
                                            paramPos += args[i].length();
                                        } else {
                                            paramPos++;
                                        }
                                    }
                                }

                                result.replace(pos, endPos - pos, body);
                                pos += body.length();
                                changed = true;
                                continue;
                            }
                        }
                    }
                    pos++;
                }
            }
        }
    }

    return result;
}

std::string GlslPreprocessor::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string GlslPreprocessor::stripComments(const std::string& code) {
    std::string result;
    bool inBlockComment = false;
    bool inLineComment = false;

    for (size_t i = 0; i < code.length(); ++i) {
        if (inBlockComment) {
            if (i + 1 < code.length() && code[i] == '*' && code[i + 1] == '/') {
                inBlockComment = false;
                i++;
            }
        } else if (inLineComment) {
            if (code[i] == '\n') {
                inLineComment = false;
                result += '\n';
            }
        } else {
            if (i + 1 < code.length() && code[i] == '/' && code[i + 1] == '*') {
                inBlockComment = true;
                i++;
            } else if (i + 1 < code.length() && code[i] == '/' && code[i + 1] == '/') {
                inLineComment = true;
                i++;
            } else {
                result += code[i];
            }
        }
    }

    return result;
}

std::vector<std::string> GlslPreprocessor::tokenize(const std::string& str, const std::string& delims) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find_first_of(delims);

    while (end != std::string::npos) {
        if (end > start) {
            tokens.push_back(str.substr(start, end - start));
        }
        start = end + 1;
        end = str.find_first_of(delims, start);
    }

    if (start < str.length()) {
        tokens.push_back(str.substr(start));
    }

    return tokens;
}

std::string GlslPreprocessor::extractString(const std::string& str, size_t start) {
    if (start >= str.length() || str[start] != '"') {
        return "";
    }

    size_t end = str.find('"', start + 1);
    if (end == std::string::npos) {
        return "";
    }

    return str.substr(start + 1, end - start - 1);
}
