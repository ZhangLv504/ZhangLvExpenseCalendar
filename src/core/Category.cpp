#include "Category.h"

namespace zl {

QVector<QString> Category::expenseNames()
{
    return {
        QStringLiteral("餐饮"), QStringLiteral("交通"), QStringLiteral("购物"),
        QStringLiteral("居住"), QStringLiteral("娱乐"), QStringLiteral("医疗"),
        QStringLiteral("教育"), QStringLiteral("人情"), QStringLiteral("其他")
    };
}

QVector<QString> Category::incomeNames()
{
    return {
        QStringLiteral("工资"), QStringLiteral("奖金"), QStringLiteral("兼职"),
        QStringLiteral("理财"), QStringLiteral("红包"), QStringLiteral("其他")
    };
}

bool Category::isBuiltInExpense(const QString &name)
{
    return expenseNames().contains(name);
}

bool Category::isBuiltInIncome(const QString &name)
{
    return incomeNames().contains(name);
}

} // namespace zl
