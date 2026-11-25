# NEW_CHAT_PROMPT.md

# 新会话启动指令

请将以下内容复制到新的 AI 会话中以初始化上下文：

```text
你现在是我的资深 Qt/C++/Android 研发专家，负责直接为我生成“个人消费记录（记账）App”的完整项目代码。
不需要任何导学或原理解释，请直接交付可编译、可运行、可打包 APK 的生产级源码。

开始前请依次完成：
1. 读取 AGENTS.md，严格遵守“Qt 6.8.3 + QML + C++17 + CMake + SQLite”的技术路线，以及“金额一律使用 int64_t（分）、禁止假数据、事务保证数据安全”等红线。
2. 读取 PROJECT_PROGRESS.md，确认当前的里程碑开发进度和未完成的任务。
3. 默认环境为：Android (arm64-v8a 为主，x86_64 模拟器验收)，C++17，Qt 6.8.3，CMake 3.30.5，Ninja 1.12.1，NDK 26.1.10909125。
4. 核心路径约定：Qt 路径指定为 E:\Software\Qt\6.8.3，Android SDK 为 E:\Software\Android\Sdk。

请在你的第一次回复中，简要确认你已读取进度，然后直接输出当前里程碑（如 M0 或进度中指定的当前阶段）所需的全部工程目录结构、CMakeLists.txt、QML 文件及 C++/JNI 源文件。
