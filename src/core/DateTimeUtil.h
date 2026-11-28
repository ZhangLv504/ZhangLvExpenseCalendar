#pragma once

#include <cstdint>
#include <QString>

namespace zl {

// 账目日期的解析与格式化工具（纯 C++，可单测）。
// 用户录入与展示统一为 "yyyy-MM-dd"（年月日，无时分）；核心层仍以 Unix 毫秒时间戳流转，
// 新增账目取所选日期的本地 00:00，历史含时分的数据按本地日期归属，读时只显示年月日。
class DateTimeUtil final {
public:
    DateTimeUtil() = delete;

    // 账目录入用的严格日期解析：仅接受零填充的 yyyy-MM-dd，空串/非法日期返回 false。
    // 成功时输出所选日期的本地 00:00 毫秒时间戳。
    static bool parseDate(const QString &text, std::int64_t *outMs);

    // 将毫秒时间戳格式化为 "yyyy-MM-dd"（按本地时区取日期）。
    static QString formatDate(std::int64_t ms);

    // 旧版 "yyyy-MM-dd HH:mm" 解析/格式化，仅保留供历史数据与既有测试使用；
    // 记账入口不再调用。
    static bool parse(const QString &text, std::int64_t *outMs);
    static QString format(std::int64_t ms);
};

} // namespace zl
