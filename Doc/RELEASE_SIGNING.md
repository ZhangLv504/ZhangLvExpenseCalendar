# 正式签名与密钥保管说明

> 从 1.0.0（versionCode 8）起，交付包使用正式签名证书。旧版为 Android Debug 公共证书，
> 两者证书不同，Android 会视为两个不同应用——**必须先卸载旧版才能安装新版，且旧版数据不会自动迁移**。

## 1. 密钥库位置与口令

| 项 | 值 |
| -- | -- |
| 密钥库文件 | `signing/zhanglv-release.jks`（PKCS12，RSA 2048，有效期 10000 天） |
| 别名 | `zhanglv` |
| 口令文件 | `signing/keystore.properties`（`storePassword` / `keyPassword`） |
| 证书 SHA-256 | `533e05f46ca8969eb648d06973ebb1cbf4354fb122c54d9f7a3aea64ea911ee2` |
| 证书 DN | `CN=ZhangLv Expense, OU=Personal, O=ZhangLv, L=Beijing, ST=Beijing, C=CN` |

`signing/` 已被 `.gitignore` 排除，**密钥库与口令绝不入库**。

## 2. 必须长期保管

- 密钥库丢失 = 永远无法以同一身份发布升级，已安装用户只能卸载重装并丢失数据。Google 也无法找回。
- 口令泄露 = 他人可签名同包名应用冒充本应用。
- 保管要求：
  1. 将 `signing/` 整个目录复制到至少两个**离线**位置（如加密 U 盘 + 私有云盘），并确认口令文件与密钥库一起备份。
  2. 口令与密钥库分开存放更稳妥，但两者都必须可找回。
  3. 换电脑、重装系统、格盘前，先确认备份仍可读取。
  4. 不要把密钥库放进 Git、网盘公开目录、聊天记录或代码托管平台。

## 3. 构建正式签名包

Qt 6.8.3 的 CMake 生成命令不支持直接传入 `--sign`，且 Gradle 配置签名会让 `androiddeployqt`
找不到预期的未签名文件名。当前采用两步法（已验证）：

```bash
# 1) Release 配置构建，产出未签名包
cmake -S . -B build-android-arm64-release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=E:/Software/Qt/6.8.3/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_SDK_ROOT=E:/Software/Android/Sdk \
  -DANDROID_NDK_ROOT=E:/Software/Android/Sdk/ndk/26.1.10909125 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android-arm64-release -j4

# 2) 用正式密钥签名（口令从 signing/keystore.properties 读取到变量，不要硬编码到脚本）
apksigner sign --ks signing/zhanglv-release.jks --ks-key-alias zhanglv \
  --ks-pass pass:<口令> --key-pass pass:<口令> \
  --out dist/zhanglv-expense-1.0.0-arm64-v8a-release.apk \
  build-android-arm64-release/android-build/build/outputs/apk/release/android-build-release-unsigned.apk

# 3) 验证
apksigner verify --verbose --print-certs dist/zhanglv-expense-1.0.0-arm64-v8a-release.apk
```

`androiddeployqt` 后处理偶尔在 Release 阶段卡住（Gradle 已产出 APK 但工具未退出）。
若发生，确认 `android-build-release-unsigned.apk` 已生成后终止构建，直接执行第 2 步即可。

## 4. 旧版数据迁移（必读）

由于证书不同，升级路径如下：

1. **在旧版（0.7.0 及更早）中打开「备份 → 导出完整备份」**，保存 JSON 文件。
   - 该备份包含全部账目、自定义分类与月度预算；旧的「导出」按钮只导出当月 CSV，不含分类和预算。
2. 卸载旧版（会清空应用沙箱内数据）。
3. 安装 1.0.0 正式签名包。
4. 打开「备份 → 从备份恢复 → 选择文件并恢复」，选择第 1 步的 JSON 文件。

若跳过第 1 步，账目与分类将永久丢失。恢复操作有二次确认，且底层为单事务：
校验失败或写入失败会整体回滚，不会破坏已有数据。

## 5. 版本升级

每次发布新版本必须递增 `CMakeLists.txt` 中的 `project(VERSION ...)` 与
`QT_ANDROID_VERSION_CODE` / `QT_ANDROID_VERSION_NAME`，以及 `src/app/main.cpp` 的
`app.setApplicationVersion`。签名证书保持不变，否则用户无法覆盖升级。
