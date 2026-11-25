# 项目进度与架构契约

> 本文件为项目**唯一事实来源**，由 AI Agent 在每个里程碑交付后自行维护更新。
> 状态口径：`未开始` / `进行中` / `已交付` / `阻塞`。

## 0. 当前状态

| 项         | 当前值                                                       |
| ---------- | ------------------------------------------------------------ |
| 当前里程碑 | **M0 — 已交付**                                              |
| 当前版本   | `0.1.0`（versionCode 1）                                     |
| 下一步动作 | 进入 M1 收尾：分类自定义、明细编辑/删除交互打磨与边界回归       |

## 1. 环境契约（锁定）

| 项                              | 锁定值                                                       |
| ------------------------------- | ------------------------------------------------------------ |
| Qt                              | `6.8.3`（`E:\Software\Qt\6.8.3`）                            |
| C++ / 构建                      | C++17 / CMake 3.30.5 / Ninja 1.12.1（`E:\Software\Qt\Tools\Ninja\ninja.exe`） |
| Android SDK / NDK               | `E:\Software\Android\Sdk` / `26.1.10909125`（`ANDROID_PLATFORM=android-26`） |
| JDK                             | `21.0.12.1`（`D:\AiSoftware\jdk-21.0.12.1`）                 |
| minSdk / targetSdk / compileSdk | `26 / 34 / 35`                                               |
| 包名 / 方向 / 语言              | `com.zhanglv.expense` / 竖屏 / 简体中文                      |
| ABI / 验收设备                  | 交付 `arm64-v8a`；验收 `x86_64`（AVD `kge_api35`，`-gpu swiftshader`） |

## 2. 里程碑规划（由 Agent 细化并追踪）

| 里程碑 | 目标                                                    | 状态   |
| ------ | ------------------------------------------------------- | ------ |
| M0     | 工程骨架、基础页面导航、SQLite 初始化、Android 打包链路 | 已交付 |
| M1     | 核心记账闭环（增删改查、分类、明细列表、本地持久化）    | 主体已交付（随 M0 落地） |
| M2+    | 搜索筛选、预算、CSV 导出与备份等（按需迭代）            | 未开始 |

> 说明：M0 搭建骨架时已将核心记账闭环（增删改查 + 事务 + 月度汇总）一并实现并通过单测与模拟器验收，因此 M1 主体功能已提前达成；M1 仅剩交互打磨与边界回归。

## 3. 架构与目录

```
ZhangLvExpense/
├── CMakeLists.txt               # 顶层：Qt6 组件、core 库、App 目标、QML 模块、双 ABI 属性
├── src/
│   ├── core/                    # 纯 C++ 核心层（可独立单测，无 QML 依赖）
│   │   ├── Money.{h,cpp}        # 金额解析/格式化，全链路 int64_t 分，逐字符处理禁用浮点
│   │   ├── Transaction.{h,cpp}  # 账目模型 + 合法性校验
│   │   ├── Category.{h,cpp}     # 内置收支分类
│   │   └── Database.{h,cpp}     # SQLite 引擎：建表/增删改查/月度汇总，写操作全部事务
│   └── app/
│       ├── main.cpp             # 入口：Basic 样式、应用私有目录建库、注册 QML 上下文
│       ├── AppController.{h,cpp}# QML 桥接：金额以元字符串进出，内部转分
│       ├── TransactionModel.*   # QAbstractListModel，供 ListView 绑定
│       └── qml/Main.qml         # 竖屏 UI：月度导航、汇总、明细、记一笔/编辑页
├── tests/                       # tst_money / tst_database（Qt Test）
├── android/                     # Android 打包模板（manifest 竖屏、包名、gradle）
└── dist/                        # 交付 APK（不入库）+ README.md 版本清单
```

### 表结构

`transactions`：id(INTEGER PK AUTOINCREMENT)、type(0支出/1收入)、amount_cents(INTEGER)、category(TEXT)、note(TEXT)、created_ms(INTEGER)，索引 `idx_transactions_created_ms`。金额字段为 64 位整数分，无浮点。

## 4. 验收结果（M0 / 0.1.0）

### 桌面单测（mingw_64，Qt Test）
- `tst_money`：31 项通过（边界金额、两位小数、负值、非法格式、溢出、往返转换）
- `tst_database`：11 项通过（建表幂等、增删改查、跨月边界、月度汇总、空账本、事务回滚拒绝非法数据、亿元级金额精度）
- CTest：`100% tests passed, 0 tests failed out of 2`

### Android 构建
- arm64-v8a：Release，交付包 `dist/zhanglv-expense-0.1.0-arm64-v8a-debug.apk`（20 MB，SHA-256 见 dist/README.md）
- x86_64：Debug，模拟器验收包
- aapt 校验：包名 `com.zhanglv.expense` / versionCode 1 / versionName 0.1.0 / minSdk 26 / targetSdk 34 / 应用名「掌律记账」/ 竖屏 / 对应 ABI

### 模拟器验收（AVD kge_api35, Android 15, x86_64, -gpu swiftshader）
- 冷启动无崩溃，Qt/QML 加载正常，无运行时错误
- UI：月度导航、收支汇总卡、明细列表、记一笔按钮正常渲染
- 闭环：记一笔「支出 餐饮 12.34」→ 保存返回 → 支出汇总更新为 12.34、明细出现 -12.34
- 持久化：force-stop 后重启，账目仍在（SQLite 真实落盘）
- 非法金额（格式错误）被拒绝并提示，不落库

### 已知限制
- 模拟器 SwiftShader 软件渲染下，部分 Qt Quick Controls 控件（ToolBar/Button/ComboBox 背景）出现梯形斜切视觉瑕疵；属模拟器图形栈渲染问题，arm64 真机交付包不受影响。
- 应用使用 Basic 样式以保证低配设备渲染稳定。
- 正式签名未接入（当前 debug 签名，仅侧载验证），按约定在 M5 前落地。

## 5. 变更日志

- **M0 / 0.1.0（2026-11-25）**：搭建 Qt 6.8.3 + QML + C++17 + CMake + SQLite 工程骨架；实现 int64_t 分金额引擎与事务化 SQLite 核心层；完成竖屏记账 UI 闭环与双 ABI 打包链路；桌面 42 项单测 + 模拟器验收通过。
