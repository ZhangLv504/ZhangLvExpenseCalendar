#include "Money.h"

#include <QLatin1Char>

namespace zl {
namespace {

constexpr std::int64_t kMax = INT64_MAX;
constexpr std::int64_t kMin = INT64_MIN;

} // namespace

bool Money::parse(const QString &text, std::int64_t *outCents)
{
    if (!outCents)
        return false;

    const QString s = text.trimmed();
    if (s.isEmpty())
        return false;

    int i = 0;
    bool negative = false;
    if (s.at(0) == QLatin1Char('-')) {
        negative = true;
        ++i;
    } else if (s.at(0) == QLatin1Char('+')) {
        ++i;
    }
    if (i >= s.size())
        return false;

    std::int64_t whole = 0;
    int wholeDigits = 0;
    while (i < s.size() && s.at(i).isDigit()) {
        const int d = s.at(i).digitValue();
        if (whole > (kMax - d) / 10)
            return false; // 整数部分溢出
        whole = whole * 10 + d;
        ++wholeDigits;
        ++i;
    }

    std::int64_t frac = 0;   // 两位小数，例如 ".05" -> 5
    int fracDigits = 0;
    if (i < s.size() && s.at(i) == QLatin1Char('.')) {
        ++i;
        while (i < s.size() && s.at(i).isDigit()) {
            if (fracDigits >= 2) {
                // 第三位小数：直接拒绝，不做四舍五入，保证账目精确
                return false;
            }
            frac = frac * 10 + s.at(i).digitValue();
            ++fracDigits;
            ++i;
        }
    }

    if (i != s.size())
        return false; // 残留非法字符
    if (wholeDigits == 0 && fracDigits == 0)
        return false; // 只有符号或只有小数点

    if (fracDigits == 1)
        frac *= 10; // ".5" -> 50 分

    // whole * 100 + frac，带溢出检查
    if (whole > (kMax - frac) / 100)
        return false;
    const std::int64_t magnitude = whole * 100 + frac;

    if (negative) {
        // -kMax 可表示；若 magnitude 恰为 kMax+1 不可能到达，这里仍做防御
        if (magnitude > static_cast<std::int64_t>(kMax))
            return false;
        *outCents = -magnitude;
    } else {
        *outCents = magnitude;
    }
    return true;
}

QString Money::format(std::int64_t cents)
{
    const bool negative = cents < 0;
    // 用无符号取绝对值，避免 INT64_MIN 取负溢出
    std::uint64_t mag;
    if (negative)
        mag = static_cast<std::uint64_t>(-(cents + 1)) + 1;
    else
        mag = static_cast<std::uint64_t>(cents);

    const std::uint64_t whole = mag / 100;
    const int frac = static_cast<int>(mag % 100);
    const QString fracText = QStringLiteral("%1").arg(frac, 2, 10, QLatin1Char('0'));

    return QStringLiteral("%1%2.%3")
        .arg(negative ? QStringLiteral("-") : QString())
        .arg(QString::number(whole))
        .arg(fracText);
}

std::int64_t Money::parseOrZero(const QString &text, bool *ok)
{
    std::int64_t cents = 0;
    const bool good = parse(text, &cents);
    if (ok)
        *ok = good;
    return good ? cents : 0;
}

} // namespace zl
