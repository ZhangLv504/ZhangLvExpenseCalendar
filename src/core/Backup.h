#pragma once

#include <cstdint>
#include <QByteArray>
#include <QVector>

#include "Category.h"
#include "Transaction.h"


namespace zl {

// 预算条目（单位：分），用于完整备份
struct BudgetRecord {
    int year = 0;
    int month = 0;
    int type = 0;      // 0 支出 / 1 收入
    std::int64_t amountCents = 0;
};

// 一次完整账本快照：账目 + 分类 + 预算
struct BackupData {
    int schemaVersion = 1;
    QVector<Transaction> transactions;
    QVector<CategoryRecord> categories;
    QVector<BudgetRecord> budgets;
};

// 账本完整备份的 JSON 序列化/反序列化（纯 C++，可单测）。
// 金额一律以“分”的整数字符串写入 JSON，避免浮点与大整数精度丢失；
// 时间戳以毫秒整数字符串写入。解析时严格校验字段与数值范围，
// 任何字段非法即整体失败，不返回半成品。
class Backup final {
public:
    Backup() = delete;

    static constexpr int kCurrentSchemaVersion = 1;

    // 序列化为 UTF-8 JSON；写入失败返回空字节数组。
    static QByteArray build(const BackupData &data);

    // 解析 JSON；成功返回 true 并填充 out，失败返回 false 并给出中文错误。
    static bool parse(const QByteArray &json, BackupData *out, QString *error = nullptr);
};

} // namespace zl
