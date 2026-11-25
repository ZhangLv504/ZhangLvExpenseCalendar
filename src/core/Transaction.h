#pragma once

#include <cstdint>
#include <QString>

namespace zl {

// 账目类型：支出 / 收入
enum class TxType : int {
    Expense = 0,
    Income = 1
};

// 一条账目记录。amountCents 恒为非负的“分”，收支由 type 区分。
struct Transaction {
    std::int64_t id = 0;              // 0 表示尚未入库（sqlite rowid 从 1 开始）
    TxType type = TxType::Expense;
    std::int64_t amountCents = 0;     // 单位：分，恒 >= 0
    QString category;                 // 分类名
    QString note;                     // 备注
    std::int64_t createdMs = 0;       // Unix 毫秒时间戳（本地录入时刻）
};

// 基本合法性校验：金额非负、分类非空、时间戳非负。
bool isValidTransaction(const Transaction &tx);

} // namespace zl
