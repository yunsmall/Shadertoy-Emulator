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
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

// 静态成员初始化 - 默认使用外部预处理器
GlslPreprocessor::Mode GlslPreprocessor::s_defaultMode = GlslPreprocessor::Mode::External;

void GlslPreprocessor::reset() {
    m_macros.clear();
    m_includedFiles.clear();
    m_conditionStack.clear();
}

namespace {
// 只想知道 glslangValidator 能不能跑起来，不关心它输出什么，
// 所以直接起个进程看退出码，不经过 shell，也不用管道。
#ifdef _WIN32
bool probeExternalValidator() {
    // CreateProcessW 可能改动命令行缓冲，不能用字符串字面量
    wchar_t cmdLine[] = L"glslangValidator --version";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    // CREATE_NO_WINDOW 给它一个隐藏控制台，--version 的输出不会冒到这边的终端
    if (CreateProcessW(nullptr, cmdLine, nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi) == 0) {
        return false;  // 找不到可执行文件
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}
#else
bool probeExternalValidator() {
    // 子进程的输出丢到 /dev/null，只留退出码
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    char* const argv[] = {const_cast<char*>("glslangValidator"),
                          const_cast<char*>("--version"), nullptr};
    pid_t pid = 0;
    // 带 p 后缀的版本会按 PATH 查找，找不到直接返回错误码
    const int rc = posix_spawnp(&pid, "glslangValidator", &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0) {
        return false;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif
}  // namespace

// 探测一次外部预处理器是否可用，结果缓存下来。
// 用不了时退回内置预处理器：外部程序缺失会让预处理返回空串，
// 而调用方只会看到 shader 编译失败，很难查到根因。
bool GlslPreprocessor::externalValidatorAvailable() {
    static const bool available = [] {
        const bool found = probeExternalValidator();
        if (!found) {
            std::cerr << "glslangValidator not found in PATH, falling back to built-in preprocessor" << std::endl;
        }
        return found;
    }();
    return available;
}

bool GlslPreprocessor::shouldUseExternal() const {
    return m_mode == Mode::External && externalValidatorAvailable();
}

std::string GlslPreprocessor::process(const std::string& code,
                                       const std::filesystem::path& basePath,
                                       int maxIncludeDepth) {
    if (shouldUseExternal()) {
        return runExternalPreprocessor(code, basePath);
    }
    reset();
    return processCode(code, basePath, maxIncludeDepth);
}

// ========== 外部预处理器实现 ==========

std::string GlslPreprocessor::runExternalPreprocessor(const std::string& code,
                                                       const std::filesystem::path& basePath) {
    // 临时文件（放在 basePath 目录下，这样 #include 相对路径能正确工作）。
    // 名字带上进程号：固定名字在两份配置同时跑同一个目录时会互相踩
#ifdef _WIN32
    const unsigned long pid = GetCurrentProcessId();
#else
    const unsigned long pid = static_cast<unsigned long>(getpid());
#endif
    const std::filesystem::path tempFile =
        basePath / (".glsl_preprocess_" + std::to_string(pid) + ".frag");

    // 写入代码。glslangValidator 默认不认 #include，得显式请求 GL_GOOGLE_include_directive
    {
        std::ofstream out(tempFile, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "GLSL Preprocessor: cannot create temp file" << std::endl;
            return code;
        }
        out << "#extension GL_GOOGLE_include_directive : enable\n" << code;
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
    const bool succeeded = (_pclose(pipe) == 0);
#else
    const int status = pclose(pipe);
    const bool succeeded = WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif

    // 删除临时文件
    std::filesystem::remove(tempFile);

    // 失败时它把报错和半成品一起堆在 stdout 上，直接当结果返回的话，下游只会看到
    // "shader 编译失败"，真正的原因（哪一行、什么错）就埋了。这里报出来并返回空串，
    // 和内置预处理器失败时的行为对齐
    if (!succeeded) {
        std::cerr << "GLSL Preprocessor: glslangValidator failed" << std::endl;
        // 具体错误它多半已经写到 stderr 了（会直接漏到终端），但有些情况只在 stdout，
        // 这里补上，免得两头都不显示
        if (!output.empty()) {
            std::cerr << output << std::endl;
        }
        return "";
    }

    // 把为了 include 才加的扩展声明剔掉，否则 GPU 驱动会对不认识的扩展报警告
    std::string cleaned;
    std::istringstream input(output);
    std::string line;
    while (std::getline(input, line)) {
        if (line.find("GL_GOOGLE_include_directive") == std::string::npos) {
            cleaned += line;
            cleaned += '\n';
        }
    }
    return cleaned;
}

// ========== 内置预处理器实现 ==========

std::string GlslPreprocessor::processCode(const std::string& code,
                                           const std::filesystem::path& currentDir,
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
                    std::string included = processInclude(args, currentDir, depth - 1);
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
                                              const std::filesystem::path& currentDir,
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
    std::filesystem::path fullPath = currentDir / includePath;
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
    return processCode(buffer.str(), fullPath.parent_path(), depth);
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
