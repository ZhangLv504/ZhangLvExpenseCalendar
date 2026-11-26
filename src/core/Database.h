#pragma once

#include <cstdint>
#include <QString>
#include <QVector>

#include "Transaction.h"

class QSqlDatabase;

namespace zl {

// 月度收支汇总，单位：分
struct MonthSummary {
    std::int64_t incomeCents = 0;
    std::int64_t expenseCents = 0;
    int count = 0;
};

// 分类条目：builtin 为 true 表示内置分类（不可删除）
struct CategoryRecord {
    QString name;
    int type = 0;      // 0 支出 / 1 收入，对齐 TxType
    bool builtin = false;
};

// 分类月度汇总条目（单位：分）
struct CategoryStat {
    QString category;
    std::int64_t totalCents = 0;
    int count = 0;
};

// 月度趋势点（单位：分），供近 N 月收支柱状对比
struct MonthPoint {
    int year = 0;
    int month = 0;
    std::int64_t incomeCents = 0;
    std::int64_t expenseCents = 0;
};

// 单一账本的 SQLite 存储引擎。
// 线程约定：实例只在创建它的线程使用；写操作全部包裹在事务中，失败自动回滚。
class Database final {
public:
    explicit Database(const QString &dbPath);
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    QString path() const;
    bool isOpen() const;

    // 打开（必要时创建父目录）并建表；幂等。
    bool open(QString *error = nullptr);

    // 新增一条账目，成功时回填 id 与 createdMs。
    bool insert(Transaction *tx, QString *error = nullptr);

    // 按 id 更新整条账目。
    bool update(const Transaction &tx, QString *error = nullptr);

    // 按 id 删除。
    bool remove(std::int64_t id, QString *error = nullptr);

    // 按 id 读取。
    bool get(std::int64_t id, Transaction *out, QString *error = nullptr) const;

    // 查询指定年月的全部账目（按本地时间月初到月末），按时间倒序。
    QVector<Transaction> queryByMonth(int year, int month, QString *error = nullptr) const;

    // 带筛选的月度查询：type 取 -1 表示全部类型；category 为空表示不限分类；
    // keyword 非空时在分类与备注中做包含匹配（内部转义 LIKE 通配符）。按时间倒序。
    QVector<Transaction> queryByMonthFiltered(int year, int month, int type,
                                              const QString &category,
                                              const QString &keyword,
                                              QString *error = nullptr) const;

    // 指定年月收支汇总。
    MonthSummary monthSummary(int year, int month, QString *error = nullptr) const;

    // 指定年月、指定类型的分类汇总，按合计金额降序（金额相同按分类名升序）。
    QVector<CategoryStat> monthCategoryStats(int year, int month, int type,
                                             QString *error = nullptr) const;

    // 全部账目条数。
    int count(QString *error = nullptr) const;

    // ---- 分类管理 ----
    // 列出指定类型的全部分类（内置在前，自定义在后），内置分类在首次打开时自动播种。
    QVector<CategoryRecord> categories(int type, QString *error = nullptr) const;

    // 新增自定义分类；名称不能为空、不能与同类型已有分类重名。
    bool addCategory(int type, const QString &name, QString *error = nullptr);

    // 删除自定义分类；内置分类不可删，已被账目引用的分类不可删。
    bool removeCategory(int type, const QString &name, QString *error = nullptr);

    // ---- 月度预算 ----
    // 设置/更新预算（金额单位为分，须 >= 0）；按 (year, month, type) upsert。
    bool setBudget(int year, int month, int type, std::int64_t amountCents,
                   QString *error = nullptr);
    // 读取预算；未设置返回 0 且 *found=false。
    std::int64_t budget(int year, int month, int type, bool *found = nullptr,
                        QString *error = nullptr) const;
    // 删除指定预算。
    bool removeBudget(int year, int month, int type, QString *error = nullptr);

    // 含当前月在内的近 N 个月收支趋势，按时间升序（旧→新）。
    QVector<MonthPoint> monthSeries(int year, int month, int count,
                                    QString *error = nullptr) const;

private:
    bool seedBuiltinCategories(class QSqlDatabase &db, QString *error);

    class Private;
    Private *d;
};

} // namespace zl
