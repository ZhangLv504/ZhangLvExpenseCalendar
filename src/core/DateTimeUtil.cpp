#include "DateTimeUtil.h"

#include <QDateTime>
#include <QLatin1Char>

namespace zl {
namespace {

constexpr const char *kPattern = "yyyy-MM-dd HH:mm";

} // namespace

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
