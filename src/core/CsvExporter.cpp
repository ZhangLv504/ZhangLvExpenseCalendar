#include "CsvExporter.h"

#include <QLatin1Char>

#include "DateTimeUtil.h"
#include "Money.h"

namespace zl {
namespace {

QString typeText(TxType type)
{
    return type == TxType::Income ? QStringLiteral("收入") : QStringLiteral("支出");
}

} // namespace

QString CsvExporter::escapeField(const QString &text, bool guardFormula)
{
    QString value = text;

    // 公式注入防护：Excel/WPS 会把以这些字符开头的单元格当公式执行。
    // 前置单引号使其变为纯文本；仅对文本字段（分类/备注）启用，金额列保持数值语义。
    if (guardFormula && !value.isEmpty()) {
        const QChar first = value.at(0);
        if (first == QLatin1Char('=') || first == QLatin1Char('+')
            || first == QLatin1Char('-') || first == QLatin1Char('@')) {
            value.prepend(QLatin1Char('\''));
        }
    }

    const bool needsQuote = value.contains(QLatin1Char(','))
        || value.contains(QLatin1Char('"'))
        || value.contains(QLatin1Char('\n'))
        || value.contains(QLatin1Char('\r'));

    if (!needsQuote)
        return value;

    // RFC4180：内部双引号翻倍，整体用双引号包裹
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    value.prepend(QLatin1Char('"'));
    value.append(QLatin1Char('"'));
    return value;
}

QByteArray CsvExporter::build(const QVector<Transaction> &items)
{
    QStringList lines;
    lines << QStringLiteral("日期,类型,分类,金额(元),备注");

    for (const Transaction &tx : items) {
        const QString dateText = DateTimeUtil::formatDate(tx.createdMs);
        const QString amountText = Money::format(tx.amountCents); // 金额恒为正，无负号

        const QStringList cells = {
            escapeField(dateText, false),
            escapeField(typeText(tx.type), false),
            escapeField(tx.category, true),
            escapeField(amountText, false),
            escapeField(tx.note, true)
        };
        lines << cells.join(QLatin1Char(','));
    }

    // UTF-8 BOM：Excel 直接双击打开中文不乱码
    QByteArray out("\xEF\xBB\xBF");
    out.append(lines.join(QStringLiteral("\r\n")).toUtf8());
    out.append("\r\n");
    return out;
}

} // namespace zl
