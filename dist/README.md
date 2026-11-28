# 交付包版本清单

APK 不入库，仅在本机 `dist/` 保留备份。本文件记录每个交付包的版本、校验和与标签，便于核对。

| 版本 | versionCode | ABI | 文件 | SHA-256 | 标签 | 备注 |
| ---- | ----------- | --- | ---- | ------- | ---- | ---- |
| 0.1.0 | 1 | arm64-v8a | `zhanglv-expense-0.1.0-arm64-v8a-debug.apk` | `a07988dbc0cecb0a7bc959b225c93a8076626631e0b0d530f541834a46588752` | `v0.1.0` | M0 交付包，debug 签名，20 MB |
| 0.2.0 | 2 | arm64-v8a | `zhanglv-expense-0.2.0-arm64-v8a-debug.apk` | `7113eb10f33fb176f1d4df4c21dfa8f30c93fe2ebe13b82f9bc5834b3aaf7b82` | `v0.2.0` | M1 收尾：自定义分类、删除入口、日期时间录入，debug 签名，20 MB |
| 0.3.0 | 3 | arm64-v8a | `zhanglv-expense-0.3.0-arm64-v8a-debug.apk` | `597905578a11d99811ead13098f58ac27f64450af885a198146873a894511cac` | `v0.3.0` | M2：明细搜索筛选、分类统计、空态修复，debug 签名，约 19.3 MiB |
| 0.4.0 | 4 | arm64-v8a | `zhanglv-expense-0.4.0-arm64-v8a-debug.apk` | `6a5853d85976916e87d60d8fc469105c1cb841ad778257074ca24851840ffa27` | `v0.4.0` | M3：CSV 导出（SAF）、公式注入防护，debug 签名，约 22 MiB |
| 0.5.0 | 5 | arm64-v8a | `zhanglv-expense-0.5.0-arm64-v8a-debug.apk` | `bf34538b4c7c4eed313f5acc52fb60d7898c58a415ea43add32dce0a01738828` | `v0.5.0` | M4：月度预算与超支提示、结余/日均支出统计；应用名「张律记账」+ 自适应图标，Debug 签名有效，约 25 MiB |
| 0.6.0 | 6 | arm64-v8a | `zhanglv-expense-0.6.0-arm64-v8a-debug.apk` | `fb836c50c46cd1949bdaaa16f68c1fb696c2ed8005cf659960df86796aaa5999` | `v0.6.0` | M4.1：日历视图（按天看收支）、月份上限、编辑页日期间距；Debug 签名有效，约 25 MiB |
| 0.7.0 | 7 | arm64-v8a | `zhanglv-expense-0.7.0-arm64-v8a-debug.apk` | `4c4f01a2ab9484e02fa6117f7ff4ab971c48976ffda834816b5569766552ddf8` | `v0.7.0` | M4.2：记账日期仅年月日 + 滚轮选择器，CSV 同步改日期；Debug 签名有效，约 24 MiB |
| 1.0.0 | 8 | arm64-v8a | `zhanglv-expense-1.0.0-arm64-v8a-release.apk` | `9b5788f23b919e1c00c9d0d2bb92124afccca5f605db7fcb1d753b58d9dbe1ff` | `v1.0.0` | M5：正式签名（v2/v3，RSA-2048，证书指纹 533e05f4…）、完整备份/恢复；与旧版证书不同，必须卸载旧版并先导出备份，约 22 MiB |

> x86_64 包仅用于模拟器验收（`build-android-x86_64/android-build/ZhangLvExpense.apk`），不列入交付清单。
>
> **签名分界**：0.1.0–0.7.0 为 Android Debug 公共证书侧载包（需按当时流程用 Debug 配置构建）；
> **1.0.0 起为正式签名包**（Release 构建 + apksigner 用项目私钥签名，v2/v3 方案）。两者证书不同，
> Android 视为不同应用，**必须先卸载旧版再装新版**，旧数据不会自动迁移，须用「备份 → 导出完整备份 / 从备份恢复」搬运。
> 详见 `Doc/RELEASE_SIGNING.md`。密钥库与口令不入库，丢失无法恢复。
