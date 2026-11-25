# 开发环境依赖与项目启动指南

> 本文记录开发过程中安装的**全部工具与库**、可复现的**安装命令**、每个依赖的**作用**，
> 以及**如何从零构建并启动当前项目**。
>
> - 环境锁定值以 [`Doc/PROJECT_PROGRESS.md`](PROJECT_PROGRESS.md) 第 1 节为准（唯一事实来源）。
> - 第三方依赖的许可证、来源、ABI 验证登记在 [`third_party/DEPENDENCIES.md`](../third_party/DEPENDENCIES.md)。
> - 当前版本：**0.3.0 / versionCode 3（M2 已交付）**。
> - 最后更新：2026-11-25。

---

## 1. 环境锁定值（不得擅自变更）

| 项                              | 锁定值                                           | 路径 / 说明                                                |
| ------------------------------- | ------------------------------------------------ | ---------------------------------------------------------- |
| Qt                              | **6.8.3**                                        | `E:\Software\Qt\6.8.3`，全项目唯一版本，不混用 5.15 / 6.5  |
| 主目标 Kit                      | `android_arm64_v8a`                              | 首期交付 ABI                                               |
| 模拟器 Kit                      | `android_x86_64`                                 | 仅验收用                                                   |
| 桌面调试 Kit                    | `mingw_64` + `Tools\mingw1310_64`                | UI 预览与纯 C++ 单测，不作为交付物                         |
| CMake                           | 3.30.5                                           | `E:\Software\Qt\Tools\CMake_64\bin\cmake.exe`              |
| Ninja                           | 1.12.1                                           | `E:\Software\Qt\Tools\Ninja\ninja.exe`，**不在系统 PATH**  |
| C++ 标准                        | **C++17**                                        | 不用 C++20，规避 NDK 26 + Qt 6.8.3 组合风险                |
| Android SDK 根                  | `E:\Software\Android\Sdk`                        | 8 个包齐全                                                 |
| NDK                             | **26.1.10909125**                                | 编译期原生 API 上限为 34，故 `ANDROID_PLATFORM=android-26` |
| JDK                             | **21.0.12.1**                                    | `D:\AiSoftware\jdk-21.0.12.1`，需显式设置 `JAVA_HOME`      |
| OpenSSL for Android             | `E:\Software\Android\Sdk\android_openssl`        | Qt Creator 检测绿勾；本应用纯离线，运行期不依赖            |
| minSdk / targetSdk / compileSdk | 26 / 34 / 35                                     | 与 Qt 6.8.3 Android 模板保持一致                           |
| 包名 / 方向 / 语言              | `com.zhanglv.expense` / 竖屏 / 简体中文          | 首次发版后包名不可变更                                     |
| 验收设备                        | Android 15 模拟器（Pixel 6 / x86_64）            | 使用本机既有 x86_64 AVD，WHPX 加速可用                     |
| 签名                            | Android Debug key                                | 交付包仅侧载验证；正式签名在 M5 前落地                     |
| ABI 策略                        | 交付 arm64-v8a，验收 x86_64                      | 两套构建目录分开，单一 APK 无法兼顾                        |

---

## 2. 依赖总表

### 2.1 基础工具链（沿用本机已装环境）

| 名称           | 版本（实测） | 作用                                                         | 安装 / 获取说明                                              |
| -------------- | ------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| Qt for Android | 6.8.3        | QML 界面、C++ 框架、SQLite 驱动、Android 打包（`androiddeployqt`） | 已安装于 `E:\Software\Qt`，含 `android_arm64_v8a`、`android_x86_64`、`mingw_64` 及 CMake/Ninja/MinGW |
| JDK            | 21.0.12.1    | Gradle / `androiddeployqt` / `adb` 所需 Java 运行时          | 已安装于 `D:\AiSoftware\jdk-21.0.12.1`，需设置 `JAVA_HOME`   |
| Python         | 3.x          | 可选：生成大批量测试账单 CSV/JSON 验收素材                   | 系统安装，无需项目级虚拟环境                                 |

### 2.2 Android SDK / NDK 包（可复现命令）

```bat
set JAVA_HOME=D:\AiSoftware\jdk-21.0.12.1
set ANDROID_SDK_ROOT=E:\Software\Android\Sdk
"%ANDROID_SDK_ROOT%\cmdline-tools\latest\bin\sdkmanager.bat" ^
  "platforms;android-35" "build-tools;35.0.0" "platform-tools" ^
  "cmdline-tools;latest" "ndk;26.1.10909125" "emulator" ^
  "extras;google;usb_driver" "system-images;android-35;google_apis_playstore;x86_64"
