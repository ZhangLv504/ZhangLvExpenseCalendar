#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "Database.h"
#include "TransactionModel.h"

namespace zl {

// QML 与核心层之间的唯一桥接对象：所有记账操作经由它调用 SQLite 引擎。
// 金额对外一律以“元字符串”交互，内部解析为分；不向 QML 暴露浮点金额。
class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(TransactionModel *model READ model CONSTANT)
    Q_PROPERTY(QString monthIncomeText READ monthIncomeText NOTIFY summaryChanged)
    Q_PROPERTY(QString monthExpenseText READ monthExpenseText NOTIFY summaryChanged)
    Q_PROPERTY(int currentYear READ currentYear WRITE setCurrentYear NOTIFY monthChanged)
    Q_PROPERTY(int currentMonth READ currentMonth WRITE setCurrentMonth NOTIFY monthChanged)
    Q_PROPERTY(int filterType READ filterType WRITE setFilterType NOTIFY filterChanged)
    Q_PROPERTY(QString searchKeyword READ searchKeyword WRITE setSearchKeyword NOTIFY filterChanged)
    Q_PROPERTY(bool filterActive READ filterActive NOTIFY filterChanged)
    Q_PROPERTY(QString dbPath READ dbPath CONSTANT)

public:
    explicit AppController(Database *db, QObject *parent = nullptr);

    TransactionModel *model() const;
    QString monthIncomeText() const;
    QString monthExpenseText() const;
    int currentYear() const;
    int currentMonth() const;
    int filterType() const;          // -1 全部 / 0 支出 / 1 收入
    QString searchKeyword() const;
    bool filterActive() const;       // 是否处于筛选状态（列表空态文案区分）
    QString dbPath() const;

    void setCurrentYear(int year);
    void setCurrentMonth(int month);
    void setFilterType(int type);
    void setSearchKeyword(const QString &keyword);

    // 初始化数据库并加载当前月
    Q_INVOKABLE bool initialize(QString *error = nullptr);

    // 新增：amountText 为元字符串（如 "12.34"）；dateText 格式 "yyyy-MM-dd"（年月日），
    // 为空则用当天。返回错误信息，空串表示成功。
    Q_INVOKABLE QString addRecord(int type, const QString &amountText,
                                  const QString &category, const QString &note,
                                  const QString &dateText = QString());

    // 更新指定行（按模型行号定位 id），dateText 语义同上
    Q_INVOKABLE QString updateRecord(int row, int type, const QString &amountText,
                                     const QString &category, const QString &note,
                                     const QString &dateText = QString());

    // 删除指定行
    Q_INVOKABLE QString deleteRecord(int row);

    // 查询某行数据，返回 {txId,type,amountText,category,note,dateText}
    Q_INVOKABLE QVariantMap recordAt(int row) const;

    Q_INVOKABLE QStringList expenseCategories() const;
    Q_INVOKABLE QStringList incomeCategories() const;

    // 月度分类统计（type：0 支出 / 1 收入），返回
    // [{category, amountText, count, percent}]，percent 为该分类占同类型本月合计的整数百分比。
    Q_INVOKABLE QVariantList monthCategoryStats(int type) const;

    // 本月指定类型的合计金额（元字符串），供统计页顶部展示。
    Q_INVOKABLE QString monthTotalText(int type) const;

    // 月度预算：未设置时 budgetText 返回空串；setBudget 的 amountText 为元字符串，
    // 传空串表示清除预算。
    Q_INVOKABLE QString budgetText(int type) const;
    // 预算卡展示数据，金额全部以分在 C++ 算好：
    // {budgetText, hint, over(bool), percent(int)}
    Q_INVOKABLE QVariantMap budgetInfo(int type) const;
    Q_INVOKABLE QString setBudget(int type, const QString &amountText);
    // 结余（收入-支出，可为负）与日均支出（按当月天数），均为元字符串。
    Q_INVOKABLE QString monthBalanceText() const;
    Q_INVOKABLE QString dailyAverageText() const;

    // 日历数据：monthDailyMap 返回 {"3": {incomeText, expenseText, count}, ...}；
    // transactionsOnDay(day) 返回当天账目 [{type,amountText,category,note,dateText}]。
    Q_INVOKABLE QVariantMap monthDailyMap() const;
    Q_INVOKABLE QVariantList transactionsOnDay(int day) const;
    // 是否允许前进到下一月（不超过系统当前月）。
    Q_INVOKABLE bool canGoNextMonth() const;

    // 分类管理（写入 SQLite 持久化）
    // categoryDetails 返回 [{name, builtin}]，供管理页标注内置分类不可删
    Q_INVOKABLE QVariantList categoryDetails(int type) const;
    Q_INVOKABLE QString addCategory(int type, const QString &name);
    Q_INVOKABLE QString removeCategory(int type, const QString &name);

    // 导出指定年月账目为 CSV。Android 走系统文件选择器（SAF），桌面走原生保存对话框；
    // 返回空串表示用户取消或成功（细节经 exportFinished 通知），非空为错误信息。
    Q_INVOKABLE QString exportMonthCsv(int year, int month);

public slots:
    void refresh();

signals:
    void summaryChanged();
    void monthChanged();
    void filterChanged();
    void categoriesChanged();
    void budgetChanged();
    void exportFinished(bool ok, const QString &message);
    void errorOccurred(const QString &message);

private:
    void reloadCurrentMonth();

    Database *m_db;
    TransactionModel *m_model;
    int m_year;
    int m_month;
    int m_filterType;          // -1 全部 / 0 支出 / 1 收入
    QString m_searchKeyword;
    std::int64_t m_incomeCents;
    std::int64_t m_expenseCents;
};

} // namespace zl
