#include "glsl_translator.hpp"

#include <ANGLE/ShaderLang.h>

#include <mutex>

namespace {

// ANGLE 的全局初始化做一次就够
void ensureInitialized() {
    static std::once_flag once;
    std::call_once(once, [] { sh::Initialize(); });
}

}  // namespace

TranslatedShader translateShader(const std::string& esslSource) {
    TranslatedShader result;
    ensureInitialized();

    ShBuiltInResources resources;
    sh::InitBuiltInResources(&resources);
    resources.FragmentPrecisionHigh = 1;

    ShCompileOptions options = {};
    options.objectCode = true;
    options.initializeUninitializedLocals = true;

    // 0x8B30 就是 GL_FRAGMENT_SHADER。sh::GLenum 用的是 GL 那套枚举值，但为了
    // 一个常量去引 GLES2/gl2.h 会连带拉进一整套 EGL/GLES 声明
    const auto kFragmentShader = static_cast<sh::GLenum>(0x8B30);
    ShHandle compiler = sh::ConstructCompiler(kFragmentShader, SH_WEBGL2_SPEC,
                                              SH_GLSL_330_CORE_OUTPUT, &resources);
    if (!compiler) {
        result.log = "ConstructCompiler failed";
        return result;
    }

    const char* sources[] = {esslSource.c_str()};
    result.ok = sh::Compile(compiler, sources, 1, options);
    if (result.ok) {
        result.code = sh::GetObjectCode(compiler);
        // 变量的原名和驱动里实际的名字都在这张表里，不用自己去猜前缀规则
        if (const std::vector<sh::ShaderVariable>* vars = sh::GetUniforms(compiler)) {
            for (const sh::ShaderVariable& var : *vars) {
                result.uniforms[var.name] = var.mappedName;
            }
        }
    } else {
        result.log = sh::GetInfoLog(compiler);
    }

    sh::Destruct(compiler);
    return result;
}
