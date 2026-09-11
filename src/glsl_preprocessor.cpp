#include "glsl_preprocessor.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
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
    m_includedFiles.clear();
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
        // 扩展声明占掉的这一行会计进后面的行号里（glslangValidator 是按本文件的行号
        // 编 #line 的），加一句 #line 1 把它之后的内容拉回第 1 行，报错的行号才对着
        // shader 自己数
        out << "#extension GL_GOOGLE_include_directive : enable\n"
            << "#line 1\n"
            << code;
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

    // 把为了 include 才加的扩展声明剔掉，否则 GPU 驱动会对不认识的扩展报警告。
    // 但位置要留着（换成空行）：glslangValidator 是按"临时文件第几行"编的 #line，
    // 整行抽掉会让它后面所有行号少 1，报错就指到上一行去了
    std::string cleaned;
    std::istringstream input(output);
    std::string line;
    while (std::getline(input, line)) {
        if (line.find("GL_GOOGLE_include_directive") == std::string::npos) {
            cleaned += line;
        }
        cleaned += '\n';
    }
    return cleaned;
}

// ========== 内置预处理器实现 ==========

// 只展开 #include，别的指令原样透传（连行内容和顺序都不动）。
//
// 之前这里还自己实现宏展开和条件编译，是个填不满的坑：GLSL 的预处理器支持
// #if 的表达式（defined()、比较、&&、嵌套），而这边只认 #ifdef/#ifndef，
// 于是 #if 块既不参与条件栈、又会被后面的 #endif 误弹，#endif 那一行还被吃掉，
// 结果两个分支的代码全都留着——轻则重复定义，重则编译器报"未闭合的 #if"直接编不过。
// 现在这些统统交给 GLSL 编译器，它本来就有完整的预处理器，而且比手写的对
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
    size_t lineNo = 0;

    while (std::getline(input, line)) {
        ++lineNo;
        const std::string trimmedLine = trim(line);

        if (!trimmedLine.empty() && trimmedLine[0] == '#') {
            const size_t spacePos = trimmedLine.find_first_of(" \t");
            const std::string directive = (spacePos != std::string::npos)
                                          ? trimmedLine.substr(0, spacePos)
                                          : trimmedLine;

            if (directive == "#include") {
                const std::string args = (spacePos != std::string::npos)
                                         ? trim(trimmedLine.substr(spacePos + 1))
                                         : "";
                // 展开结果自带换行，这一行本身就不用再输出了
                output << processInclude(args, currentDir, depth - 1);
                // 被包含的文件长短不一，之后的行号会整体偏掉。#line 把下一行拉回
                // 本文件的行号，编译报错才指得准（GLSL 的 #line 只吃行号和源串号，
                // 不像 C 那样能跟一个文件名）
                output << "#line " << (lineNo + 1) << "\n";
                continue;
            }

            // 为了 #include 才写的扩展声明不能留给驱动：桌面 GL 没有
            // GL_GOOGLE_include_directive 这个扩展，而且 #extension 出现在
            // #version 之后本来就非法。外部那条路最后也做了同样的剔除
            if (trimmedLine.find("GL_GOOGLE_include_directive") != std::string::npos) {
                continue;
            }
        }

        output << line << "\n";
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

std::string GlslPreprocessor::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}
