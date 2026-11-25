# 真机操作安全红线（硬约束）

> **任何会话、任何阶段都不得越过本节。** 用户对真机安全有明确顾虑（「怕手机寄了」）。
> 本项目只接触**应用层**，真机风险来自刷机/Root，不来自调试。
> 应用包名基准：**`com.zhanglv.expense`**（与 CMakeLists.txt、已交付 APK 一致）。

## 1. 允许的操作（只读，或应用沙箱内，卸载即还原）

| 命令 | 用途 |
| ---- | ---- |
| `adb devices` | 查看已连接设备 |
| `adb shell getprop` | 读取设备属性 |
| `adb logcat` | 读取日志 |
| `adb shell dumpsys` | 读取系统服务状态 |
| `adb install` / `adb install -r` | 安装/覆盖安装本应用 |
| `adb uninstall com.zhanglv.expense` | 卸载**本应用** |
| `adb shell am start` / `am force-stop` | 启动/强制结束本应用 |
| `adb shell screencap` / `screenrecord` | 截图/录屏 |
| `adb pull` | 从设备取文件到本机 |
| `adb push`（仅 `/sdcard/Download` 等用户目录） | 向设备用户目录推送文件 |

## 2. 绝对禁止的操作（会真正损伤设备或数据）

- `fastboot` **任意**子命令：`flash` / `erase` / `format` / `oem unlock`
- 解锁 Bootloader、刷 Recovery、刷 ROM
- Root：`adb root` / `su` / Magisk
- `adb shell pm clear`（除本应用 `com.zhanglv.expense` 外）、`adb shell rm -rf`、恢复出厂设置
- `adb sideload`、修改系统分区
- `adb shell settings put`（改系统设置）
- 要求用户开启「OEM 解锁」「USB 调试（安全设置）」等高风险选项

## 3. 正常使用形态（向用户说明)

- 手机上只需开启「开发者选项 → USB 调试」，插线时点一次「允许」。
- 开发中的 App 运行在应用沙箱内，即使崩溃或被强制结束，也不会影响系统。
- 验证完用 `adb uninstall com.zhanglv.expense` 即可完全清除，不残留任何系统改动。

## 4. 安装/卸载正确性保障

统一通过 `scripts/device.sh` 执行，脚本内置三道校验：

1. **设备唯一性**：`adb devices` 必须恰好一台 `device` 状态设备，否则拒绝操作。
2. **包名白名单**：目标包名硬编码为 `com.zhanglv.expense`，不接受任何参数覆盖；
   安装前用 `aapt dump badging` 校验 APK 包名必须一致，ABI 必须匹配设备。
3. **卸载前后核对**：卸载前只读查询该包是否安装；卸载后再次查询确认已清除。

禁止在脚本或会话中直接手写 adb 命令绕过上述校验。若确需临时只读诊断，
只能使用第 1 节清单内的命令，且不得带任何写系统分区/清数据的参数。
