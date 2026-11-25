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
    Q_PROPERTY(QString dbPath READ dbPath CONSTANT)

public:
    explicit AppController(Database *db, QObject *parent = nullptr);

    TransactionModel *model() const;
    QString monthIncomeText() const;
    QString monthExpenseText() const;
    int currentYear() const;
    int currentMonth() const;
    QString dbPath() const;

    void setCurrentYear(int year);
    void setCurrentMonth(int month);

    // 初始化数据库并加载当前月
    Q_INVOKABLE bool initialize(QString *error = nullptr);

    // 新增：amountText 为元字符串（如 "12.34"）；返回错误信息，空串表示成功
    Q_INVOKABLE QString addRecord(int type, const QString &amountText,
                                  const QString &category, const QString &note);

    // 更新指定行（按模型行号定位 id）
    Q_INVOKABLE QString updateRecord(int row, int type, const QString &amountText,
                                     const QString &category, const QString &note);

    // 删除指定行
    Q_INVOKABLE QString deleteRecord(int row);

    // 查询某行数据，返回 {txId,type,amountText,category,note}
    Q_INVOKABLE QVariantMap recordAt(int row) const;

    Q_INVOKABLE QStringList expenseCategories() const;
    Q_INVOKABLE QStringList incomeCategories() const;

public slots:
    void refresh();

signals:
    void summaryChanged();
    void monthChanged();
    void errorOccurred(const QString &message);

private:
    void reloadCurrentMonth();

    Database *m_db;
    TransactionModel *m_model;
    int m_year;
    int m_month;
    std::int64_t m_incomeCents;
    std::int64_t m_expenseCents;
};

} // namespace zl
