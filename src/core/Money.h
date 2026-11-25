#pragma once

#include <cstdint>
#include <QString>

namespace zl {

// 金额工具：全链路以 int64_t 表示“分”，禁止使用 float/double。
// 解析与格式化均按字符串逐字符处理，不经过任何浮点运算。
class Money final {
public:
    Money() = delete;

    // 将用户输入的金额文本解析为“分”。
    // 支持：可选前导 '-'、整数部分、可选小数点 + 最多两位小数。
    // 允许 "5" / "5.2" / "5.20" / "-0.01"；拒绝空串、多余小数位、非法字符与溢出。
    static bool parse(const QString &text, std::int64_t *outCents);

    // 将“分”格式化为元字符串，固定保留两位小数，例如 1234 -> "12.34"，-5 -> "-0.05"。
    static QString format(std::int64_t cents);

    // 解析成功时返回分；失败时返回 ok == false，outCents 不变。
    static std::int64_t parseOrZero(const QString &text, bool *ok);
};

} // namespace zl
