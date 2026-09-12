// common 和 image 都会 #include 它。导出时 common 先展开，image 里这一句就该被跳过：
// 两边各展一份的话，粘到 Shadertoy 上 common 被插到通道前面就成了重复定义
#define SHARED_GAIN 0.1
