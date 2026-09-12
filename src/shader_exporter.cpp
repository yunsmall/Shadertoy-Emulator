#include "shader_exporter.hpp"
#include "glsl_preprocessor.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace {

// nlohmann 的 json 容器按 key 排序，配置里的字段顺序会被打乱；ordered_json 保持原样，
// 导出的配置读起来还和用户写的那份一样
using ordered_json = nlohmann::ordered_json;

bool writeFile(const fs::path& dst, const std::string& content) {
    std::ofstream out(dst, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Cannot write file: " << dst << std::endl;
        return false;
    }
    out << content;
    return true;
}

class ShaderExporter {
public:
    bool run(const std::string& inputPath, const std::string& outputDir, bool cleanOutput);

private:
    // 清空输出目录。输入文件就落在里面时拒绝——`--export .` 这种写法会把源文件一起
    // 删掉，删了找不回来
    bool clearOutputDir(const fs::path& input);
    bool exportConfig(const fs::path& configPath);
    bool exportSingleShader(const fs::path& shaderPath);

    // 展开 #include 后写到输出目录。basePath 是 #include 的相对解析目录，得和渲染时
    // 传的那个一致（配置所在目录），否则同一份代码在两条路上会找到不同的文件
    bool exportGlsl(const fs::path& src, const fs::path& basePath, std::string& outName);
    bool copyAsset(const fs::path& src, std::string& outName);

    // 认领输出文件名。alreadyExported 为 true 表示这个源文件之前已经导过，别写第二遍
    bool claimName(const fs::path& src, std::string& outName, bool& alreadyExported);

    fs::path m_outDir;
    // 输出文件名 -> 源文件。平铺到同一级后重名就分不清谁是谁了，得盯着
    std::map<std::string, fs::path> m_usedNames;
};

bool ShaderExporter::run(const std::string& inputPath, const std::string& outputDir,
                         bool cleanOutput) {
    const fs::path input(inputPath);
    m_outDir = outputDir;

    std::error_code ec;
    fs::create_directories(m_outDir, ec);
    if (ec) {
        std::cerr << "Cannot create output directory: " << m_outDir << " (" << ec.message() << ")\n";
        return false;
    }

    if (cleanOutput && !clearOutputDir(input)) {
        return false;
    }

    std::cout << "Exporting to " << m_outDir.string() << std::endl;

    if (input.extension() == ".json") {
        return exportConfig(input);
    }
    return exportSingleShader(input);
}

bool ShaderExporter::clearOutputDir(const fs::path& input) {
    const fs::path inputAbs = fs::weakly_canonical(input);
    const fs::path outAbs = fs::weakly_canonical(m_outDir);

    // 输入文件自己或它所在的那几层目录就是输出目录的话，清空会把源文件删掉。
    // 逐级往上比，p == p.parent_path() 是到根的终止条件（父目录等于自己）
    for (fs::path p = inputAbs.parent_path(); !p.empty(); p = p.parent_path()) {
        if (p == outAbs) {
            std::cerr << "拒绝清空 " << m_outDir.string() << "：输入文件就在这个目录里，"
                      << "清掉会把源文件一起删了\n";
            return false;
        }
        if (p == p.parent_path()) {
            break;
        }
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(m_outDir, ec)) {
        fs::remove_all(entry.path(), ec);
    }
    if (ec) {
        std::cerr << "Cannot clear output directory: " << m_outDir.string()
                  << " (" << ec.message() << ")\n";
        return false;
    }
    return true;
}

bool ShaderExporter::claimName(const fs::path& rawSrc, std::string& outName, bool& alreadyExported) {
    const fs::path src = fs::weakly_canonical(rawSrc);
    outName = src.filename().string();

    const auto it = m_usedNames.find(outName);
    if (it == m_usedNames.end()) {
        m_usedNames[outName] = src;
        alreadyExported = false;
        return true;
    }

    // 同一个文件被引用两次（比如一张图同时喂给两个通道）不是撞车，复用它那份
    if (it->second == src) {
        alreadyExported = true;
        return true;
    }

    std::cerr << "导出后有两个文件同名: " << it->second.string() << " 和 " << src.string()
              << " 都叫 " << outName << "，请改掉其中一个再导出\n";
    return false;
}

bool ShaderExporter::exportGlsl(const fs::path& src, const fs::path& basePath,
                                std::string& outName) {
    std::ifstream file(src, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open shader: " << src << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    bool alreadyExported = false;
    if (!claimName(src, outName, alreadyExported)) {
        return false;
    }
    if (alreadyExported) {
        return true;
    }

    // 每个文件各展各的，彼此不共享状态：这里只管把 #include 展开干净，展开之后
    // 重不重复是写 shader 的人自己的事
    GlslPreprocessor preprocessor;
    // 展开固定用内置预处理器：外部那条路（glslangValidator -E）会把宏也展开掉，
    // 那就不是"只展开 #include、别的原样不动"了
    preprocessor.setMode(GlslPreprocessor::Mode::BuiltIn);
    // 展开处不补 #line：文件自己就是要给人看的成品，多出来的行号指令只会让报错
    // 行号和编辑器里数出来的对不上
    preprocessor.setEmitLineDirectives(false);

    if (!writeFile(m_outDir / outName, preprocessor.process(buffer.str(), basePath))) {
        return false;
    }
    std::cout << "  " << src.lexically_normal().string() << " -> " << outName << std::endl;
    return true;
}

bool ShaderExporter::copyAsset(const fs::path& src, std::string& outName) {
    if (!fs::exists(src)) {
        std::cerr << "Cannot find file: " << src << std::endl;
        return false;
    }

    bool alreadyExported = false;
    if (!claimName(src, outName, alreadyExported)) {
        return false;
    }
    if (alreadyExported) {
        return true;
    }

    const fs::path dst = m_outDir / outName;
    std::error_code ec;
    // 输出目录就落在源文件旁边时（比如导出到子目录）拷到自己身上会失败，跳过
    if (!fs::exists(dst) || !fs::equivalent(src, dst, ec)) {
        ec.clear();
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "Cannot copy " << src.string() << " (" << ec.message() << ")\n";
            return false;
        }
    }

    std::cout << "  " << src.lexically_normal().string() << " -> " << outName << std::endl;
    return true;
}

bool ShaderExporter::exportConfig(const fs::path& configPath) {
    std::ifstream file(configPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open config: " << configPath << std::endl;
        return false;
    }

    ordered_json config;
    try {
        config = ordered_json::parse(file);
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }

    // #include 的相对解析目录。渲染时用的是配置所在目录而不是 shader 文件所在目录，
    // 导出得跟着走，不然同一份代码两边会找到不同的文件
    const fs::path baseDir = configPath.parent_path();

    // $schema 只管编辑器里的补全和校验，程序自己不看。导出件是要独立出去的，留着
    // 这个字段只会指到一个不在那儿的地方
    config.erase("$schema");

    if (config.contains("common") && config["common"].is_string()) {
        std::string outName;
        if (!exportGlsl(baseDir / config["common"].get<std::string>(), baseDir, outName)) {
            return false;
        }
        config["common"] = "./" + outName;
    }

    if (!config.contains("passes") || !config["passes"].is_array()) {
        std::cerr << configPath.string() << ": no passes" << std::endl;
        return false;
    }

    for (auto& pass : config["passes"]) {
        if (!pass.is_object()) {
            continue;
        }

        if (pass.contains("shader") && pass["shader"].is_string()) {
            std::string outName;
            if (!exportGlsl(baseDir / pass["shader"].get<std::string>(), baseDir, outName)) {
                return false;
            }
            pass["shader"] = "./" + outName;
        }

        if (!pass.contains("channels") || !pass["channels"].is_object()) {
            continue;
        }

        // 只有 texture 的 source 是文件路径：buffer 写的是通道名，keyboard 没有
        for (auto& [key, channel] : pass["channels"].items()) {
            if (!channel.is_object() || channel.value("type", "") != "texture") {
                continue;
            }
            // "source" 是现在的写法，"path" 是更早的写法，两个都认
            for (const char* field : {"source", "path"}) {
                if (!channel.contains(field) || !channel[field].is_string()) {
                    continue;
                }
                std::string outName;
                if (!copyAsset(baseDir / channel[field].get<std::string>(), outName)) {
                    return false;
                }
                channel[field] = "./" + outName;
            }
        }
    }

    // 配置本身也落到输出目录，文件名不变
    std::string configName;
    bool alreadyExported = false;
    if (!claimName(configPath, configName, alreadyExported)) {
        return false;
    }
    return writeFile(m_outDir / configName, config.dump(2) + "\n");
}

bool ShaderExporter::exportSingleShader(const fs::path& shaderPath) {
    if (!fs::exists(shaderPath)) {
        std::cerr << "Cannot find shader: " << shaderPath << std::endl;
        return false;
    }
    std::string outName;
    return exportGlsl(shaderPath, shaderPath.parent_path(), outName);
}

}  // namespace

bool exportShader(const std::string& inputPath, const std::string& outputDir, bool cleanOutput) {
    ShaderExporter exporter;
    return exporter.run(inputPath, outputDir, cleanOutput);
}
