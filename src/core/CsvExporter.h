#pragma once

#include <QByteArray>
#include <QStringList>
#include <QVector>

#include "Transaction.h"

namespace zl {

// 账目 CSV 导出器（纯 C++，可独立单测）。
// 输出 UTF-8（带 BOM，兼容 Excel 中文），字段遵循 RFC4180 转义，
// 文本字段做公式注入防护；金额由 Money::format 输出，不经浮点。
class CsvExporter final {
public:
    CsvExporter() = delete;

    // 生成 CSV 内容。空列表也返回仅含表头的有效 CSV。
    static QByteArray build(const QVector<Transaction> &items);

    // 将单个字段转义为 CSV 单元格文本（不含逗号包裹时的额外处理由内部完成）。
    // 公开仅为便于单测覆盖边界字符。
    static QString escapeField(const QString &text, bool guardFormula);
};

} // namespace zl
