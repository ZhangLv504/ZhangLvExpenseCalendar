#pragma once

#include <QString>
#include <QVector>

namespace zl {

// 内置分类条目（MVP）：结构预留自定义扩展字段。
struct CategoryInfo {
    QString name;
    QString icon; // 预留：图标资源名，当前为空
};

class Category final {
public:
    Category() = delete;

    static QVector<QString> expenseNames();
    static QVector<QString> incomeNames();

    static bool isBuiltInExpense(const QString &name);
    static bool isBuiltInIncome(const QString &name);
};

} // namespace zl
