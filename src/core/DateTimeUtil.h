#pragma once

#include <cstdint>
#include <QString>

namespace zl {

// 账目日期时间的解析与格式化工具（纯 C++，可单测）。
// 统一格式 "yyyy-MM-dd HH:mm"，以 Unix 毫秒时间戳在核心层流转。
class DateTimeUtil final {
public:
    DateTimeUtil() = delete;

    // 解析用户输入的日期时间文本；空串成功并返回 false（调用方用当前时间）。
    // 非法格式返回 false。严格匹配 yyyy-MM-dd HH:mm，不接受多余字符。
    static bool parse(const QString &text, std::int64_t *outMs);

    // 将毫秒时间戳格式化为 "yyyy-MM-dd HH:mm"。
    static QString format(std::int64_t ms);
};

} // namespace zl
