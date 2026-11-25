# 交付包版本清单

APK 不入库，仅在本机 `dist/` 保留备份。本文件记录每个交付包的版本、校验和与标签，便于核对。

| 版本 | versionCode | ABI | 文件 | SHA-256 | 标签 | 备注 |
| ---- | ----------- | --- | ---- | ------- | ---- | ---- |
| 0.1.0 | 1 | arm64-v8a | `zhanglv-expense-0.1.0-arm64-v8a-debug.apk` | `a07988dbc0cecb0a7bc959b225c93a8076626631e0b0d530f541834a46588752` | `v0.1.0` | M0 交付包，debug 签名，20 MB |

> x86_64 包仅用于模拟器验收（`build-android-x86_64/android-build/ZhangLvExpense.apk`），不列入交付清单。
