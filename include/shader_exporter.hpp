#pragma once

#include <string>

// 把一套 shader 导成一个自包含目录：每个 glsl 展开自己的 #include 后平铺到同一级，
// 纹理和配置 JSON 一并拷过去，JSON 里的路径字段改写成 ./xxx。
//
// 展开后每个 glsl 都能直接复制粘贴到 Shadertoy 上对应的标签页——common 是独立的一份
// （对应网站的 Common 标签页），各通道只带自己的代码。被 #include 的文件不单独输出，
// 它的内容已经在引用它的文件里了
//
// 输入可以是 config.json，也可以是单个 glsl
//
// cleanOutput 为 true 时先把输出目录清空再导：重复导出时上一轮多出来的文件会留在
// 那儿，看着像这次也导了
bool exportShader(const std::string& inputPath, const std::string& outputDir,
                  bool cleanOutput = false);
