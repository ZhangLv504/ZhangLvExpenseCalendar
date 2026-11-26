# 项目进度与架构契约

> 本文件为项目**唯一事实来源**，由 AI Agent 在每个里程碑交付后自行维护更新。
> 状态口径：`未开始` / `进行中` / `已交付` / `阻塞`。

## 0. 当前状态

| 项         | 当前值                                                       |
| ---------- | ------------------------------------------------------------ |
| 当前里程碑 | **M3 — 已交付**                                              |
| 当前版本   | `0.4.0`（versionCode 4）                                     |
| 下一步动作 | 进入 M4：预算与更多统计维度；真机操作先遵守第 1.1 节安全契约 |

## 1. 环境契约（锁定）

| 项                              | 锁定值                                                       |
| ------------------------------- | ------------------------------------------------------------ |
| Qt                              | `6.8.3`（`E:\Software\Qt\6.8.3`）                            |
| C++ / 构建                      | C++17 / CMake 3.30.5 / Ninja 1.12.1（`E:\Software\Qt\Tools\Ninja\ninja.exe`） |
| Android SDK / NDK               | `E:\Software\Android\Sdk` / `26.1.10909125`（`ANDROID_PLATFORM=android-26`） |
| JDK                             | `21.0.12.1`（`D:\AiSoftware\jdk-21.0.12.1`）                 |
| minSdk / targetSdk / compileSdk | `26 / 34 / 35`                                               |
| 包名 / 方向 / 语言              | `com.zhanglv.expense` / 竖屏 / 简体中文                      |
| ABI / 验收设备                  | 交付 `arm64-v8a`；验收 `x86_64`（Android 15 模拟器，`-gpu swiftshader`） |

### 1.1 真机操作安全契约（硬约束）

> 完整红线见 [`Doc/DEVICE_SAFETY.md`](DEVICE_SAFETY.md)，AI 会话约定见 [`Doc/AGENTS.md`](AGENTS.md)。
> 包名基准唯一：`com.zhanglv.expense`。

- 只允许只读或应用沙箱内操作；`fastboot`、解锁 Bootloader、刷 Recovery/ROM、Root、`pm clear`（非本应用）、`rm -rf`、恢复出厂、`sideload`、改系统分区/`settings put` **绝对禁止**。
- 安装/卸载统一走 `scripts/device.sh`（白名单 + 设备唯一性 + 包名校验 + 卸载前后核对），禁止手写 adb 绕过。
- 用户手机只需开「USB 调试」并点一次「允许」；验证完卸载本应用即完全还原，不残留系统改动。
- 已验证：危险命令经白名单全部拦截，`fastboot`/`pm clear`/`rm`/`root`/`sideload`/`settings put`/`install -d`/`am broadcast` 均被拒绝；包名与 ABI 校验分支正确。

## 2. 里程碑规划（由 Agent 细化并追踪）

| 里程碑 | 目标                                                    | 状态   |
| ------ | ------------------------------------------------------- | ------ |
| M0     | 工程骨架、基础页面导航、SQLite 初始化、Android 打包链路 | 已交付 |
| M1     | 核心记账闭环（增删改查、分类、明细列表、本地持久化）    | 已交付（0.2.0） |
| M2     | 明细搜索筛选、月度分类统计                              | 已交付（0.3.0） |
| M3     | CSV 导出（Android SAF，不申请存储权限）                  | 已交付（0.4.0） |
| M4     | 预算与更多统计维度                                      | 未开始 |
| M5     | 正式签名与发布打磨                                      | 未开始 |

> 说明：M0 搭建骨架时已将核心记账闭环一并实现；0.2.0 补齐 M1 收尾项——分类持久化与自定义管理、
> 编辑页删除入口（确认页）、日期时间手工录入，并在模拟器完成三条链路的交互验收。

## 3. 架构与目录

```
ZhangLvExpense/
├── CMakeLists.txt               # 顶层：Qt6 组件、core 库、App 目标、QML 模块、双 ABI 属性
├── src/
│   ├── core/                    # 纯 C++ 核心层（可独立单测，无 QML 依赖）
│   │   ├── Money.{h,cpp}        # 金额解析/格式化，全链路 int64_t 分，逐字符处理禁用浮点
│   │   ├── CsvExporter.{h,cpp}  # CSV 生成：UTF-8 BOM、RFC4180 转义、公式注入防护
│   │   ├── DateTimeUtil.{h,cpp}# "yyyy-MM-dd HH:mm" 解析/格式化，毫秒时间戳流转
│   │   ├── Transaction.{h,cpp}  # 账目模型 + 合法性校验
│   │   ├── Category.{h,cpp}     # 内置收支分类（首次打开播种进 SQLite）
│   │   └── Database.{h,cpp}     # SQLite 引擎：建表/增删改查/筛选查询/月度汇总/分类汇总与 CRUD
│   └── app/
│       ├── main.cpp             # 入口：QApplication(Basic 样式)、私有目录建库、注册 QML 上下文
│       ├── AppController.{h,cpp}# QML 桥接：金额元字符串、筛选、分类管理、日期时间、统计、SAF 导出
│       ├── TransactionModel.*   # QAbstractListModel（含 count 属性），供 ListView 绑定
│       └── qml/Main.qml         # 竖屏 UI：月度导航、汇总、搜索筛选、明细、记一笔/编辑、删除确认、分类管理、分类统计、导出按钮与轻提示
├── tests/                       # tst_money / tst_database / tst_datetime / tst_csv（Qt Test）
├── scripts/device.sh            # 真机/模拟器安全操作封装（白名单；ui 动作仅限 emulator-*）
├── android/                     # Android 打包模板（manifest 竖屏、包名、gradle）
└── dist/                        # 交付 APK（不入库）+ README.md 版本清单
```

### 表结构

`transactions`：id(INTEGER PK AUTOINCREMENT)、type(0支出/1收入)、amount_cents(INTEGER)、category(TEXT)、note(TEXT)、created_ms(INTEGER)，索引 `idx_transactions_created_ms`。金额字段为 64 位整数分，无浮点。

`categories`：id(INTEGER PK AUTOINCREMENT)、type(0支出/1收入)、name(TEXT)、builtin(INTEGER，1=内置不可删)，唯一约束 `UNIQUE(type, name)`。首次打开时在空表内播种内置分类；自定义分类不可与同类型重名，被账目引用的分类不可删。

## 4. 验收结果

### 4.0 M3 / 0.4.0（当前）

#### 桌面单测（mingw_64，Qt Test）
- `tst_money`：31 / `tst_database`：17 / `tst_datetime`：17 / `tst_csv`：7 项全部通过
- CTest：`100% tests passed, 0 tests failed out of 4`（合计 72 项）
- CSV 单测覆盖：UTF-8 BOM、表头、RFC4180 逗号/引号/换行转义、`=+-@` 公式注入防护、金额列不做防护、亿元级金额精度、空列表

#### Android 构建
- arm64-v8a：Release + debug 签名侧载包 `dist/zhanglv-expense-0.4.0-arm64-v8a-debug.apk`（约 22 MiB，SHA-256 见 dist/README.md）；体积较 0.3.0 +2.7 MiB，来自引入 QtWidgets（QFileDialog 的 Android 原生 SAF 支持）
- x86_64：Debug，模拟器验收包
- aapt 校验：包名 `com.zhanglv.expense` / versionCode 4 / versionName 0.4.0 / 对应 ABI
- 权限：不申请任何存储权限；`ACTION_CREATE_DOCUMENT` 由用户授予临时写权限

#### 模拟器验收（Android 15, x86_64, -gpu swiftshader）
- 主界面新增「导出」按钮；点击后 logcat 出现 `com.android.documentsui.picker.PickActivity`，确认 SAF 原生文件选择器启动
- 系统选择器默认文件名「掌律记账-2026-11.csv」，中文正常，位置为 Downloads，蓝色 SAVE 按钮正常
- 点击 SAVE 后文件真实落盘：`/sdcard/Download/掌律记账-2026-11.csv`（50 字节）
- 拉取文件校验：`BOM=EF-BB-BF`（UTF-8 BOM 正确），内容为 `日期时间,类型,分类,金额(元),备注`（中文表头无乱码）
- 卸载/重装走 `scripts/device.sh` 闭环通过，卸载后确认清除
- 无 QML 错误/FATAL（模拟器 SwiftShader 偶发 hwui SIGABRT 为已知图形栈问题，重启模拟器后稳定）

### 4.1 M2 / 0.3.0

#### 桌面单测（mingw_64，Qt Test）
- `tst_money`：31 项通过（边界金额、两位小数、负值、非法格式、溢出、往返转换）
- `tst_database`：17 项通过（在 M1 基础上新增类型/分类/关键字组合筛选、LIKE 通配符转义、月度分类汇总与跨月归属）
- `tst_datetime`：17 项通过（合法/非法格式、空串、往返转换、跨月跨年、闰年 2/29）
- CTest：`100% tests passed, 0 tests failed out of 3`（合计 65 项）

#### Android 构建
- arm64-v8a：Release + debug 签名侧载包 `dist/zhanglv-expense-0.3.0-arm64-v8a-debug.apk`（约 19.3 MiB，SHA-256 见 dist/README.md）
- x86_64：Debug，模拟器验收包（`build-android-x86_64`）
- aapt 校验：包名 `com.zhanglv.expense` / versionCode 3 / versionName 0.3.0 / minSdk 26 / targetSdk 34 / 竖屏 / arm64-v8a；apksigner 校验签名有效（Android Debug 证书）

#### 模拟器验收（Android 15, x86_64, -gpu swiftshader）
- 类型筛选：选「支出」仅显示 2 条支出、选「收入」仅显示 1 条收入；顶部月度汇总不受筛选影响
- 关键字搜索：`lunch` 命中备注仅剩餐饮 1 条；无结果时显示「没有符合条件的账目」
- 分类统计页：支出合计 30.00 / 共 2 笔，餐饮 20.00（67%）、交通 10.00（33%）；收入合计 5000.00 / 共 1 笔，工资 100%
- 交互回归：记一笔（支出/收入切换、分类选择、备注）、编辑页回填、删除确认页闭环均正常
- 缺陷修复：空态文案被列表内容层遮挡（0.2.0 遗留，`app.model.count` 在 QAbstractListModel 上不存在）；为 TransactionModel 补 `count` 属性，并将空态标签改为列表容器内的独立叠加层，验证通过
- logcat 无 QML 错误/FATAL

### 4.2 M1 / 0.2.0（历史）

#### 桌面单测（mingw_64，Qt Test）
- `tst_money`：31 项通过（边界金额、两位小数、负值、非法格式、溢出、往返转换）
- `tst_database`：14 项通过（建表幂等、增删改查、跨月边界、月度汇总、空账本、事务回滚、亿元级金额、分类播种/自定义增删/删除守卫）
- `tst_datetime`：17 项通过（合法/非法格式、空串、往返转换、跨月跨年、闰年 2/29）
- CTest：`100% tests passed, 0 tests failed out of 3`（合计 62 项）

#### Android 构建
- arm64-v8a：Release，交付包 `dist/zhanglv-expense-0.2.0-arm64-v8a-debug.apk`（20 MB，SHA-256 见 dist/README.md）
- x86_64：Debug，模拟器验收包（`build-android-x86_64`）
- aapt 校验：包名 `com.zhanglv.expense` / versionCode 2 / versionName 0.2.0 / minSdk 26 / targetSdk 34 / 竖屏 / 对应 ABI

#### 模拟器验收（Android 15, x86_64, -gpu swiftshader）
- 安装：`scripts/device.sh install`（设备唯一性 + 包名 + ABI 校验通过）；冷启动无崩溃，logcat 无 QML 错误/FATAL
- 分类管理：支出内置 9 类、收入内置 6 类正常显示；新增自定义分类 TestCat 成功；内置分类“删除”按钮禁用；重名新增被拒并提示“分类已存在”；删除自定义分类成功
- 日期时间：记一笔手工录入 `2026-11-20 09:15`，保存后明细时间显示正确、支出汇总 12.34
- 编辑迁月：金额改为 20.00、日期改为 `2026-10-20 09:15` 后，11 月汇总归零、10 月出现 -20.00（跨月归属正确）
- 删除：编辑页“删除”→ 确认页“确认删除”后回到明细页，10 月汇总归零
- 持久化：新增账目 5.00 与自定义分类 PersistCat 后 force-stop 重启，账目与分类均保留（SQLite 真实落盘）
- 自动化工具：`scripts/device.sh` 新增 `start/stop/shot/ui`；`ui`（tap/swipe/text/key）硬校验设备序列号为 `emulator-*`，真机一律拒绝

### 4.3 M0 / 0.1.0（历史）

#### 桌面单测（mingw_64，Qt Test）
- `tst_money`：31 项通过（边界金额、两位小数、负值、非法格式、溢出、往返转换）
- `tst_database`：11 项通过（建表幂等、增删改查、跨月边界、月度汇总、空账本、事务回滚拒绝非法数据、亿元级金额精度）
- CTest：`100% tests passed, 0 tests failed out of 2`

#### Android 构建
- arm64-v8a：Release，交付包 `dist/zhanglv-expense-0.1.0-arm64-v8a-debug.apk`（20 MB，SHA-256 见 dist/README.md）
- x86_64：Debug，模拟器验收包
- aapt 校验：包名 `com.zhanglv.expense` / versionCode 1 / versionName 0.1.0 / minSdk 26 / targetSdk 34 / 应用名「掌律记账」/ 竖屏 / 对应 ABI

#### 模拟器验收（Android 15, x86_64, -gpu swiftshader）
- 冷启动无崩溃，Qt/QML 加载正常，无运行时错误
- UI：月度导航、收支汇总卡、明细列表、记一笔按钮正常渲染
- 闭环：记一笔「支出 餐饮 12.34」→ 保存返回 → 支出汇总更新为 12.34、明细出现 -12.34
- 持久化：force-stop 后重启，账目仍在（SQLite 真实落盘）
- 非法金额（格式错误）被拒绝并提示，不落库

### 4.4 已知限制
- 模拟器 SwiftShader 软件渲染下，部分 Qt Quick Controls 控件（ToolBar/Button/ComboBox 背景）出现梯形斜切视觉瑕疵；属模拟器图形栈渲染问题，arm64 真机交付包不受影响。
- 应用使用 Basic 样式以保证低配设备渲染稳定。
- 正式签名未接入（当前 debug 签名，仅侧载验证），按约定在 M5 前落地。

## 5. 变更日志

- **M3 / 0.4.0（2026-11-26）**：新增 CsvExporter（UTF-8 BOM / RFC4180 转义 / 公式注入防护）与 7 项单测；引入 QtWidgets，用 QFileDialog 的 Android 原生 SAF（ACTION_CREATE_DOCUMENT）+ QFile 写 content:// URI 实现导出，不申请存储权限；主界面加导出按钮与轻提示；桌面 72 项单测 + 模拟器 SAF 导出落盘校验通过。
- **M2 / 0.3.0（2026-11-25）**：明细页新增类型筛选与关键字搜索（分类/备注，LIKE 通配符转义）；新增月度分类统计页（按分类汇总、笔数、占比条）；修复空态文案不显示的遗留缺陷（TransactionModel 补 count、空态独立叠加层）；桌面 65 项单测 + 模拟器筛选/统计/交互回归通过。
- **M1 / 0.2.0（2026-11-25）**：分类持久化（`categories` 表 + 内置播种 + 自定义增删与删除守卫）；新增 DateTimeUtil 支持记一笔/编辑时手工录入日期时间；编辑页补删除入口与独立确认页；`scripts/device.sh` 增加模拟器 UI 自动化（仅 emulator-*）；桌面 62 项单测 + 模拟器三条链路验收通过。
- **M0 / 0.1.0（2026-11-25）**：搭建 Qt 6.8.3 + QML + C++17 + CMake + SQLite 工程骨架；实现 int64_t 分金额引擎与事务化 SQLite 核心层；完成竖屏记账 UI 闭环与双 ABI 打包链路；桌面 42 项单测 + 模拟器验收通过。
