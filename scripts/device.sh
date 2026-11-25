#!/usr/bin/env bash
# 真机/模拟器安全操作封装。红线见 Doc/DEVICE_SAFETY.md，任何会话不得绕过本脚本手写 adb。
# 所有 adb 调用必须经过 safe_adb() 白名单；危险命令（fastboot/root/pm clear/rom flash 等）在此被硬拦截。
set -euo pipefail

# 阻止 Git Bash(MSYS) 把 /sdcard/... 等设备路径转换成 Windows 本地路径
export MSYS2_ARG_CONV_EXCL='*'

PKG="com.zhanglv.expense"                                   # 唯一包名基准，不接受外部覆盖
ADB="E:/Software/Android/Sdk/platform-tools/adb.exe"
AAPT="E:/Software/Android/Sdk/build-tools/35.0.0/aapt.exe"

die() { echo "[拒绝] $*" >&2; exit 1; }
info() { echo "[device] $*"; }

# ---------- 命令白名单：任何 adb 调用的唯一入口 ----------
safe_adb() {
    local sub="${1:-}"
    case "$sub" in
        devices|logcat|pull|push)
            ;;
        install)
            # 仅允许 install / install -r，禁止 -d 降级、-g 授权等扩展参数
            case "${2:-}" in
                ""|-r) ;;
                *) die "禁止的 install 参数: $2" ;;
            esac
            ;;
        uninstall)
            [[ "${2:-}" == "$PKG" ]] || die "卸载目标必须是 $PKG，实际为: ${2:-<空>}"
            ;;
        shell)
            case "${2:-}" in
                getprop|dumpsys|screencap|screenrecord) ;;
                input)
                    # 仅模拟器 UI 自动化验收使用，调用前必须经 require_emulator 校验
                    case "${3:-}" in
                        tap)
                            [[ "${4:-}" =~ ^[0-9]+$ && "${5:-}" =~ ^[0-9]+$ ]] \
                                || die "tap 需要两个数字坐标"
                            ;;
                        swipe)
                            [[ "${4:-}" =~ ^[0-9]+$ && "${5:-}" =~ ^[0-9]+$ \
                               && "${6:-}" =~ ^[0-9]+$ && "${7:-}" =~ ^[0-9]+$ ]] \
                                || die "swipe 需要四个数字坐标"
                            [[ "${8:-}" =~ ^[0-9]+$ ]] || die "swipe 需要持续时间(ms)"
                            ;;
                        text)
                            [[ -n "${4:-}" ]] || die "text 内容不能为空"
                            ;;
                        keyevent)
                            case "${4:-}" in
                                3|4|66|67|122|123) ;;
                                *) die "禁止的 keyevent: ${4:-<空>}（仅允许 3/4/66/67/122/123）" ;;
                            esac
                            ;;
                        *) die "禁止的 input 子命令: ${3:-<空>}" ;;
                    esac
                    ;;
                am)
                    case "${3:-}" in
                        start|force-stop) ;;
                        *) die "禁止的 am 子命令: ${3:-<空>}" ;;
                    esac
                    ;;
                pm)
                    case "${3:-}" in
                        list) ;;                         # 只允许只读的 pm list，禁止 pm clear
                        *) die "禁止的 pm 子命令: ${3:-<空>}（pm clear 等绝不允许）" ;;
                    esac
                    ;;
                *) die "禁止的 shell 命令: ${2:-<空>}" ;;
            esac
            ;;
        *)
            die "禁止的 adb 命令: $sub（fastboot/root/sideload/flash 等绝不允许）"
            ;;
    esac
    "$ADB" "$@"
}

# ---------- 前置校验：设备必须恰好一台且状态为 device ----------
require_single_device() {
    local out
    out=$(safe_adb devices)
    local n
    n=$(printf '%s\n' "$out" | awk 'NR>1 && $2=="device" {c++} END {print c+0}')
    [[ "$n" -eq 1 ]] || die "需要恰好 1 台在线设备，当前为 $n 台。请先确认连接（adb devices）。"
    DEVICE_SERIAL=$(printf '%s\n' "$out" | awk 'NR>1 && $2=="device" {print $1; exit}')
    info "设备已确认: $DEVICE_SERIAL"
}

# ---------- 前置校验：仅允许模拟器（ui 自动化验收用，真机一律拒绝） ----------
require_emulator() {
    require_single_device
    case "$DEVICE_SERIAL" in
        emulator-*) ;;
        *) die "该动作仅允许模拟器（emulator-*），当前设备: $DEVICE_SERIAL" ;;
    esac
}

# ---------- 校验 APK：包名必须匹配、ABI 必须被设备支持 ----------
verify_apk() {
    local apk="${1:-}"
    [[ -f "$apk" ]] || die "APK 文件不存在: $apk"

    local badging
    badging=$("$AAPT" dump badging "$apk" 2>/dev/null) || die "无法解析 APK: $apk"
    local apk_pkg
    apk_pkg=$(printf '%s\n' "$badging" | sed -n "s/^package: name='\([^']*\)'.*/\1/p")
    [[ "$apk_pkg" == "$PKG" ]] || die "APK 包名不符: $apk_pkg（期望 $PKG）"

    local device_abi
    device_abi=$(safe_adb shell getprop ro.product.cpu.abi)
    device_abi=$(printf '%s' "$device_abi" | tr -d '\r')
    printf '%s\n' "$badging" | grep -q "'$device_abi'" \
        || die "APK 不支持设备 ABI $device_abi（请选用匹配架构的 APK）"

    local code name
    code=$(printf '%s\n' "$badging" | sed -n "s/.*versionCode='\([^']*\)'.*/\1/p")
    name=$(printf '%s\n' "$badging" | sed -n "s/.*versionName='\([^']*\)'.*/\1/p")
    info "APK 校验通过: 包名=$apk_pkg versionCode=$code versionName=$name ABI=$device_abi"
}

# ---------- 只读查询本应用是否已安装 ----------
is_installed() {
    local out
    out=$(safe_adb shell pm list packages "$PKG")
    printf '%s\n' "$out" | grep -q "package:$PKG$"
}

# ---------- 动作 ----------
cmd_devices() {
    info "当前连接的设备（只读）:"
    safe_adb devices
}

cmd_install() {
    local apk="${1:-}"
    [[ -n "$apk" ]] || die "用法: device.sh install <APK路径>"
    require_single_device
    verify_apk "$apk"
    info "开始安装本应用（仅写应用沙箱）..."
    safe_adb install -r "$apk"
    is_installed && info "安装完成并已验证: $PKG" || die "安装后未查询到 $PKG"
}

cmd_uninstall() {
    require_single_device
    if is_installed; then
        info "检测到已安装 $PKG，开始卸载（仅移除本应用）..."
        safe_adb uninstall "$PKG"
    else
        info "设备上未安装 $PKG，无需卸载"
    fi
    if is_installed; then
        die "卸载后仍查询到 $PKG，请人工核查"
    fi
    info "卸载完成并已验证清除: $PKG（无系统残留）"
}

cmd_check() {
    require_single_device
    if is_installed; then
        info "本应用已安装: $PKG"
    else
        info "本应用未安装: $PKG"
    fi
}

# ---------- 应用启停（am start / am force-stop，均在本应用沙箱内） ----------
cmd_start() {
    require_single_device
    is_installed || die "未安装 $PKG，请先 install"
    safe_adb shell am start -n "$PKG/org.qtproject.qt.android.bindings.QtActivity"
}

cmd_stop() {
    require_single_device
    safe_adb shell am force-stop "$PKG"
    info "已强制结束: $PKG"
}

# ---------- 截图到本机（只读 screencap + pull） ----------
cmd_shot() {
    local local_path="${1:-}"
    [[ -n "$local_path" ]] || die "用法: device.sh shot <本机png路径>"
    require_single_device
    safe_adb shell screencap -p /sdcard/zl_shot.png
    safe_adb pull /sdcard/zl_shot.png "$local_path" >/dev/null
    info "截图已保存: $local_path"
}

# ---------- 模拟器 UI 自动化（真机绝对拒绝） ----------
cmd_ui() {
    require_emulator
    local action="${1:-}"
    shift || true
    case "$action" in
        tap)   [[ $# -eq 2 ]] || die "用法: device.sh ui tap <x> <y>"
               safe_adb shell input tap "$1" "$2" ;;
        swipe) [[ $# -eq 5 ]] || die "用法: device.sh ui swipe <x1> <y1> <x2> <y2> <ms>"
               safe_adb shell input swipe "$1" "$2" "$3" "$4" "$5" ;;
        text)  [[ $# -eq 1 ]] || die "用法: device.sh ui text <内容>"
               safe_adb shell input text "$1" ;;
        key)   [[ $# -eq 1 ]] || die "用法: device.sh ui key <3|4|66>"
               safe_adb shell input keyevent "$1" ;;
        *) die "未知 ui 动作: ${action:-<空>}（tap/swipe/text/key）" ;;
    esac
}

usage() {
    cat <<'EOF'
用法: device.sh <动作> [参数]
  devices              只读列出连接设备
  check                只读查询本应用是否已安装
  install <APK路径>    校验设备/包名/ABI 后安装本应用
  uninstall            严格卸载本应用 com.zhanglv.expense 并验证清除
  start / stop         启动 / 强制结束本应用
  shot <本机png路径>   截图并拉取到本机
  ui tap|swipe|text|key 仅模拟器允许的 UI 自动化（真机一律拒绝）

安全保证：所有 adb 调用经过白名单；fastboot/root/pm clear/rm -rf/刷机等命令被硬拦截。
包名固定为 com.zhanglv.expense，不接受覆盖。详见 Doc/DEVICE_SAFETY.md。
EOF
}

case "${1:-}" in
    devices)   cmd_devices ;;
    check)     cmd_check ;;
    install)   cmd_install "${2:-}" ;;
    uninstall) cmd_uninstall ;;
    start)     cmd_start ;;
    stop)      cmd_stop ;;
    shot)      cmd_shot "${2:-}" ;;
    ui)        shift; cmd_ui "$@" ;;
    -h|--help|help|"") usage ;;
    *) die "未知动作: $1（用 --help 查看用法）" ;;
esac
