#include "AppController.h"

#include <QDate>
#include <QVariant>

#include "Money.h"
#include "Category.h"

namespace zl {

AppController::AppController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_model(new TransactionModel(this))
    , m_year(QDate::currentDate().year())
    , m_month(QDate::currentDate().month())
    , m_incomeCents(0)
    , m_expenseCents(0)
{
}

TransactionModel *AppController::model() const { return m_model; }
QString AppController::monthIncomeText() const { return Money::format(m_incomeCents); }
QString AppController::monthExpenseText() const { return Money::format(m_expenseCents); }
int AppController::currentYear() const { return m_year; }
int AppController::currentMonth() const { return m_month; }
QString AppController::dbPath() const { return m_db ? m_db->path() : QString(); }

void AppController::setCurrentYear(int year)
{
    if (m_year == year) return;
    m_year = year;
    emit monthChanged();
    reloadCurrentMonth();
}

void AppController::setCurrentMonth(int month)
{
    if (month < 1 || month > 12 || m_month == month) return;
    m_month = month;
    emit monthChanged();
    reloadCurrentMonth();
}

bool AppController::initialize(QString *error)
{
    if (!m_db) {
        if (error) *error = QStringLiteral("数据库对象为空");
        return false;
    }
    if (!m_db->open(error))
        return false;
    reloadCurrentMonth();
    return true;
}

QString AppController::addRecord(int type, const QString &amountText,
                                 const QString &category, const QString &note)
{
    std::int64_t cents = 0;
    if (!Money::parse(amountText, &cents))
        return QStringLiteral("金额格式不正确");
    if (cents < 0)
        return QStringLiteral("金额不能为负数");
    if (cents == 0)
        return QStringLiteral("金额必须大于零");

    Transaction tx;
    tx.type = (type == static_cast<int>(TxType::Income)) ? TxType::Income : TxType::Expense;
    tx.amountCents = cents;
    tx.category = category.trimmed();
    tx.note = note.trimmed();

    QString error;
    if (!m_db->insert(&tx, &error)) {
        emit errorOccurred(error);
        return error;
    }
    reloadCurrentMonth();
    return QString();
}

QString AppController::updateRecord(int row, int type, const QString &amountText,
                                    const QString &category, const QString &note)
{
    std::int64_t cents = 0;
    if (!Money::parse(amountText, &cents))
        return QStringLiteral("金额格式不正确");
    if (cents <= 0)
        return QStringLiteral("金额必须大于零");

    Transaction tx = m_model->at(row);
    if (tx.id <= 0)
        return QStringLiteral("要编辑的账目不存在");

    tx.type = (type == static_cast<int>(TxType::Income)) ? TxType::Income : TxType::Expense;
    tx.amountCents = cents;
    tx.category = category.trimmed();
    tx.note = note.trimmed();

    QString error;
    if (!m_db->update(tx, &error)) {
        emit errorOccurred(error);
        return error;
    }
    reloadCurrentMonth();
    return QString();
}

QString AppController::deleteRecord(int row)
{
    const Transaction tx = m_model->at(row);
    if (tx.id <= 0)
        return QStringLiteral("要删除的账目不存在");

    QString error;
    if (!m_db->remove(tx.id, &error)) {
        emit errorOccurred(error);
        return error;
    }
    reloadCurrentMonth();
    return QString();
}

QVariantMap AppController::recordAt(int row) const
{
    const Transaction tx = m_model->at(row);
    QVariantMap map;
    map.insert(QStringLiteral("txId"), QVariant::fromValue(tx.id));
    map.insert(QStringLiteral("type"), static_cast<int>(tx.type));
    map.insert(QStringLiteral("amountText"), Money::format(tx.amountCents));
    map.insert(QStringLiteral("category"), tx.category);
    map.insert(QStringLiteral("note"), tx.note);
    return map;
}

QStringList AppController::expenseCategories() const
{
    const QVector<QString> names = Category::expenseNames();
    QStringList list;
    for (const QString &n : names)
        list.append(n);
    return list;
}

QStringList AppController::incomeCategories() const
{
    const QVector<QString> names = Category::incomeNames();
    QStringList list;
    for (const QString &n : names)
        list.append(n);
    return list;
}

void AppController::refresh()
{
    reloadCurrentMonth();
}

void AppController::reloadCurrentMonth()
{
    if (!m_db || !m_db->isOpen())
        return;

    QString error;
    const QVector<Transaction> items = m_db->queryByMonth(m_year, m_month, &error);
    if (!error.isEmpty())
        emit errorOccurred(error);
    m_model->setTransactions(items);

    const MonthSummary summary = m_db->monthSummary(m_year, m_month, &error);
    m_incomeCents = summary.incomeCents;
    m_expenseCents = summary.expenseCents;
    emit summaryChanged();
}

} // namespace zl
