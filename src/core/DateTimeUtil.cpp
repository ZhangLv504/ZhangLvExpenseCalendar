#include "DateTimeUtil.h"

#include <QDate>
#include <QDateTime>
#include <QLatin1Char>
#include <QTime>

namespace zl {
namespace {

constexpr const char *kPattern = "yyyy-MM-dd HH:mm";
constexpr const char *kDatePattern = "yyyy-MM-dd";

} // namespace

bool DateTimeUtil::parseDate(const QString &text, std::int64_t *outMs)
{
    if (!outMs)
        return false;
    const QString s = text.trimmed();
    // 严格限定 10 位零填充格式，避免 QDate::fromString 对 "2026-1-5" 的宽松解析
    if (s.size() != 10 || s.at(4) != QLatin1Char('-') || s.at(7) != QLatin1Char('-'))
        return false;

    const QDate date = QDate::fromString(s, QString::fromLatin1(kDatePattern));
    if (!date.isValid() || date.toString(QString::fromLatin1(kDatePattern)) != s)
        return false;

    *outMs = QDateTime(date, QTime(0, 0, 0)).toMSecsSinceEpoch();
    return true;
}

QString DateTimeUtil::formatDate(std::int64_t ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms).date()
        .toString(QString::fromLatin1(kDatePattern));
}

bool DateTimeUtil::parse(const QString &text, std::int64_t *outMs)
{
    if (!outMs)
        return false;
    const QString s = text.trimmed();
    if (s.isEmpty())
        return false; // 空串由调用方处理为“当前时间”

    const QDateTime dt = QDateTime::fromString(s, QString::fromLatin1(kPattern));
    if (!dt.isValid())
        return false;
    *outMs = dt.toMSecsSinceEpoch();
    return true;
}

QString DateTimeUtil::format(std::int64_t ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QString::fromLatin1(kPattern));
}

} // namespace zl
