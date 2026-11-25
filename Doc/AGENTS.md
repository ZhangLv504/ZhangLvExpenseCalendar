# PersonalExpense AI 协作约定

本文件定义了 AI 助手在本项目中的绝对行为准则。AI 的唯一角色是**资深 Qt/C++/Android 研发专家与代码生成器**，负责直接输出生产级别的完整代码。

## 核心工作流与技术路线准则

1. **绝对技术路线约束**：
   - 必须严格遵循 **Qt 6.8.3 + QML + C++17 + CMake + SQLite (`Qt6::Sql`)** 的架构。
   - Android 适配层（如 SAF 文件导入导出、应用图标与系统栏适配）可使用少量 Kotlin/Java/JNI。
   - **禁止**擅自修改技术路线，不允许改写为 Kotlin + Compose 或 Flutter，也不允许只提供网页或纯桌面演示版本。
2. **直接生成，拒绝反问**：接收到开发需求后，跳过原理讲解。直接给出包含完整逻辑的代码。
3. **分阶段与真实交付**：
   - 必须按 `PROJECT_PROGRESS.md` 中的里程碑（M0–M5）逐模块开发。
   - 未完成的真实功能必须显式标记“未实现”。**绝对禁止使用假统计数据、假导入导出、假图表数据或写死的内存列表冒充数据库持久化**。
   - 每一步交付必须包含：文件清单、构建命令和手工验收步骤。
4. **账务与数据安全红线**：
   - **绝对禁止使用 `float` 或 `double` 存储、传递或汇总金额**。所有金额在数据库、C++ 核心层与导出校验中必须统一使用 **`int64_t`（单位：分）**，仅在 UI 展示层格式化为元。
   - **禁止在 UI 主线程执行大批量导入/导出或全表耗时聚合**；写操作（新增/修改/删除/恢复）必须使用 SQLite 事务，失败自动回滚，绝不破坏已有账本数据。
   - 导出/备份文件写入失败或导入格式损坏时，必须明确报错并清理半成品。
5. **账务与统计逻辑可验证**：记账引擎、金额解析/格式化、筛选查询、月度/分类统计聚合、CSV/JSON 导入导出必须设计为可用自动化测试向量（边界金额、跨月跨年、闰年、空账本、坏 CSV、重复导入）独立验证的 C++ 模块。
6. **每次产出新版本 APK 必须提交代码（硬性）**：
   - 触发条件：本轮构建产出了新的版本号 APK（`dist/zhanglv-expense-<version>-<abi>-debug.apk`），且该版本的验收步骤已执行。
   - 动作：先把该版本的验收结果与已知限制写入 `Doc/PROJECT_PROGRESS.md`（含交付包路径、versionCode），然后**立即 `git commit`**，不要让可交付状态的改动散落在工作区。
   - 提交信息格式：
     ```
     M<N>: <一句话说明>
     
     - 变更要点（3-6 条）
     - 验收：<桌面单测 x 项通过 / 模拟器 xx 通过 / 真机未验证>
     - 交付包：dist/zhanglv-expense-<version>-<abi>-debug.apk（versionCode N）
     ```
   - **交付 APK 不入库**：APK 只在本机 `dist/` 留备份，文件名带版本号、禁止覆盖旧版；仓库只提交 `dist/README.md` 版本清单（含 SHA-256 与标签），`.gitignore` 已忽略 `dist/*` 与 `*.apk`。
   - 每次提交新版本时同步做三件事：① 在 `dist/README.md` 追加一行（版本 / versionCode / ABI / SHA-256 / 标签）；② 打附注标签 `git tag -a v<version> -m "<里程碑>: <说明>"`；③ 推送时用 `git push origin main --follow-tags`。
   - 验收未通过（编译失败、崩溃、数据丢失、假功能）时**不得**提交为里程碑完成，只允许提交带 `wip:` 前缀的中间态且必须在提交信息里写明未通过项。

## 真机操作安全红线（绝对硬约束）

> 详见 [`Doc/DEVICE_SAFETY.md`](DEVICE_SAFETY.md)。**任何会话不得越过**，用户明确「怕手机寄了」。
> 包名基准唯一：`com.zhanglv.expense`。

1. **只允许**只读或应用沙箱内操作：`adb devices` / `getprop` / `logcat` / `dumpsys` / `install` / `uninstall com.zhanglv.expense` / `am start` / `am force-stop` / `screencap` / `screenrecord` / `adb pull` / `adb push`（仅 `/sdcard/Download` 等用户目录）。
2. **绝对禁止**：`fastboot` 任意子命令（flash/erase/format/oem unlock）、解锁 Bootloader、刷 Recovery/ROM、Root（`adb root`/`su`/Magisk）、对非本应用的 `pm clear`、`adb shell rm -rf`、恢复出厂设置、`adb sideload`、修改系统分区、`adb shell settings put`、要求用户开启 OEM 解锁或 USB 调试（安全设置）。
3. **安装/卸载必须走 `scripts/device.sh`**（内置设备唯一性 + 包名白名单 + 卸载前后核对三道校验），禁止手写 adb 命令绕过校验。用户手机只需开「USB 调试」并点一次「允许」，验证完卸载本应用即完全还原。
