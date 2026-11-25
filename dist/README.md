# 交付包版本清单

APK 不入库，仅在本机 `dist/` 保留备份。本文件记录每个交付包的版本、校验和与标签，便于核对。

| 版本 | versionCode | ABI | 文件 | SHA-256 | 标签 | 备注 |
| ---- | ----------- | --- | ---- | ------- | ---- | ---- |
| 0.1.0 | 1 | arm64-v8a | `zhanglv-expense-0.1.0-arm64-v8a-debug.apk` | `a07988dbc0cecb0a7bc959b225c93a8076626631e0b0d530f541834a46588752` | `v0.1.0` | M0 交付包，debug 签名，20 MB |
| 0.2.0 | 2 | arm64-v8a | `zhanglv-expense-0.2.0-arm64-v8a-debug.apk` | `7113eb10f33fb176f1d4df4c21dfa8f30c93fe2ebe13b82f9bc5834b3aaf7b82` | `v0.2.0` | M1 收尾：自定义分类、删除入口、日期时间录入，debug 签名，20 MB |
| 0.3.0 | 3 | arm64-v8a | `zhanglv-expense-0.3.0-arm64-v8a-debug.apk` | `597905578a11d99811ead13098f58ac27f64450af885a198146873a894511cac` | `v0.3.0` | M2：明细搜索筛选、分类统计、空态修复，debug 签名，约 19.3 MiB |

> x86_64 包仅用于模拟器验收（`build-android-x86_64/android-build/ZhangLvExpense.apk`），不列入交付清单。
