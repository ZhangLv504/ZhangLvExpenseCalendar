#include "AppController.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QVariant>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <QUrl>

#include "Money.h"
#include "Category.h"
#include "DateTimeUtil.h"
#include "CsvExporter.h"

namespace zl {

AppController::AppController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_model(new TransactionModel(this))
    , m_year(QDate::currentDate().year())
    , m_month(QDate::currentDate().month())
    , m_filterType(-1)
    , m_incomeCents(0)
    , m_expenseCents(0)
{
}

TransactionModel *AppController::model() const { return m_model; }
QString AppController::monthIncomeText() const { return Money::format(m_incomeCents); }
QString AppController::monthExpenseText() const { return Money::format(m_expenseCents); }
int AppController::currentYear() const { return m_year; }
int AppController::currentMonth() const { return m_month; }
int AppController::filterType() const { return m_filterType; }
QString AppController::searchKeyword() const { return m_searchKeyword; }
bool AppController::filterActive() const
{
    return m_filterType != -1 || !m_searchKeyword.trimmed().isEmpty();
}
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

void AppController::setFilterType(int type)
{
    const int normalized = (type == 0 || type == 1) ? type : -1;
    if (m_filterType == normalized) return;
    m_filterType = normalized;
    emit filterChanged();
    reloadCurrentMonth();
}

void AppController::setSearchKeyword(const QString &keyword)
{
    if (m_searchKeyword == keyword) return;
    m_searchKeyword = keyword;
    emit filterChanged();
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
                                 const QString &category, const QString &note,
                                 const QString &dateText)
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
    if (!dateText.trimmed().isEmpty()
        && !DateTimeUtil::parse(dateText, &tx.createdMs))
        return QStringLiteral("日期格式不正确，应为 年-月-日 时:分");

    QString error;
    if (!m_db->insert(&tx, &error)) {
        emit errorOccurred(error);
        return error;
    }
    reloadCurrentMonth();
    return QString();
}

QString AppController::updateRecord(int row, int type, const QString &amountText,
                                    const QString &category, const QString &note,
                                    const QString &dateText)
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
    if (!dateText.trimmed().isEmpty()
        && !DateTimeUtil::parse(dateText, &tx.createdMs))
        return QStringLiteral("日期格式不正确，应为 年-月-日 时:分");

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
    map.insert(QStringLiteral("dateText"), DateTimeUtil::format(tx.createdMs));
    return map;
}

QStringList AppController::expenseCategories() const
{
    QStringList list;
    if (!m_db || !m_db->isOpen())
        return list;
    const QVector<CategoryRecord> recs = m_db->categories(
        static_cast<int>(TxType::Expense), nullptr);
    for (const CategoryRecord &c : recs)
        list.append(c.name);
    return list;
}

QStringList AppController::incomeCategories() const
{
    QStringList list;
    if (!m_db || !m_db->isOpen())
        return list;
    const QVector<CategoryRecord> recs = m_db->categories(
        static_cast<int>(TxType::Income), nullptr);
    for (const CategoryRecord &c : recs)
        list.append(c.name);
    return list;
}

QVariantList AppController::monthCategoryStats(int type) const
{
    QVariantList list;
    if (!m_db || !m_db->isOpen())
        return list;

    const QVector<CategoryStat> stats = m_db->monthCategoryStats(m_year, m_month, type, nullptr);
    std::int64_t total = 0;
    for (const CategoryStat &s : stats)
        total += s.totalCents;

    for (const CategoryStat &s : stats) {
        // 占比只用于展示，非金额；整数四舍五入
        const int percent = total > 0
            ? static_cast<int>((s.totalCents * 100 + total / 2) / total)
            : 0;
        QVariantMap m;
        m.insert(QStringLiteral("category"), s.category);
        m.insert(QStringLiteral("amountText"), Money::format(s.totalCents));
        m.insert(QStringLiteral("count"), s.count);
        m.insert(QStringLiteral("percent"), percent);
        list.append(m);
    }
    return list;
}

QString AppController::monthTotalText(int type) const
{
    return Money::format(type == static_cast<int>(TxType::Income) ? m_incomeCents : m_expenseCents);
}

QString AppController::budgetText(int type) const
{
    if (!m_db || !m_db->isOpen())
        return QString();
    bool found = false;
    const std::int64_t cents = m_db->budget(m_year, m_month, type, &found, nullptr);
    return found ? Money::format(cents) : QString();
}

QVariantMap AppController::budgetInfo(int type) const
{
    QVariantMap m;
    m.insert(QStringLiteral("over"), false);
    m.insert(QStringLiteral("percent"), 0);

    if (!m_db || !m_db->isOpen()) {
        m.insert(QStringLiteral("budgetText"), QString());
        m.insert(QStringLiteral("hint"), QString());
        return m;
    }

    bool found = false;
    const std::int64_t budget = m_db->budget(m_year, m_month, type, &found, nullptr);
    if (!found || budget <= 0) {
        m.insert(QStringLiteral("budgetText"), QString());
        m.insert(QStringLiteral("hint"), QStringLiteral("未设置本月预算"));
        return m;
    }

    const std::int64_t used = (type == static_cast<int>(TxType::Income))
        ? m_incomeCents : m_expenseCents;
    // 先除后乘，避免 used*100 溢出 int64；整数百分比，可 >100
    const int percent = static_cast<int>((used / budget) * 100 + (used % budget) * 100 / budget);

    m.insert(QStringLiteral("budgetText"), Money::format(budget));
    m.insert(QStringLiteral("percent"), percent);
    if (used > budget) {
        m.insert(QStringLiteral("over"), true);
        m.insert(QStringLiteral("hint"),
                 QStringLiteral("已超支 ") + Money::format(used - budget));
    } else {
        m.insert(QStringLiteral("hint"),
                 QStringLiteral("剩余 ") + Money::format(budget - used)
                 + QStringLiteral("（已用 %1%）").arg(percent));
    }
    return m;
}

QString AppController::setBudget(int type, const QString &amountText)
{
    if (!m_db)
        return QStringLiteral("数据库未初始化");

    QString error;
    if (amountText.trimmed().isEmpty()) {
        if (!m_db->removeBudget(m_year, m_month, type, &error)) {
            emit errorOccurred(error);
            return error;
        }
    } else {
        std::int64_t cents = 0;
        if (!Money::parse(amountText, &cents))
            return QStringLiteral("预算金额格式不正确");
        if (cents < 0)
            return QStringLiteral("预算不能为负数");
        if (!m_db->setBudget(m_year, m_month, type, cents, &error)) {
            emit errorOccurred(error);
            return error;
        }
    }
    emit budgetChanged();
    return QString();
}

QString AppController::monthBalanceText() const
{
    return Money::format(m_incomeCents - m_expenseCents);
}

QString AppController::dailyAverageText() const
{
    const int days = QDate(m_year, m_month, 1).daysInMonth();
    return Money::format(m_expenseCents / days);
}

QVariantList AppController::categoryDetails(int type) const
{
    QVariantList list;
    if (!m_db || !m_db->isOpen())
        return list;
    const QVector<CategoryRecord> recs = m_db->categories(type, nullptr);
    for (const CategoryRecord &c : recs) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), c.name);
        m.insert(QStringLiteral("builtin"), c.builtin);
        list.append(m);
    }
    return list;
}

QString AppController::addCategory(int type, const QString &name)
{
    if (!m_db)
        return QStringLiteral("数据库未初始化");
    QString error;
    if (!m_db->addCategory(type, name, &error)) {
        emit errorOccurred(error);
        return error;
    }
    emit categoriesChanged();
    return QString();
}

QString AppController::removeCategory(int type, const QString &name)
{
    if (!m_db)
        return QStringLiteral("数据库未初始化");
    QString error;
    if (!m_db->removeCategory(type, name, &error)) {
        emit errorOccurred(error);
        return error;
    }
    emit categoriesChanged();
    return QString();
}

QString AppController::exportMonthCsv(int year, int month)
{
    if (!m_db || !m_db->isOpen())
        return QStringLiteral("数据库未初始化");

    QString error;
    const QVector<Transaction> items = m_db->queryByMonth(year, month, &error);
    if (!error.isEmpty())
        return error;

    const QByteArray csv = CsvExporter::build(items);

    // 默认文件名；Android 上平台文件对话框会用 ACTION_CREATE_DOCUMENT (SAF)，
    // 用户自选保存位置并授予临时写权限，无需任何存储权限。
    const QString fileName = QStringLiteral("掌律记账-%1-%2.csv")
        .arg(year).arg(month, 2, 10, QLatin1Char('0'));
    const QString docDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QUrl defaultUrl = QUrl::fromLocalFile(QDir(docDir).filePath(fileName));

    const QUrl target = QFileDialog::getSaveFileUrl(
        nullptr,
        QStringLiteral("导出 %1年%2月 账目").arg(year).arg(month),
        defaultUrl,
        QStringLiteral("CSV 文件 (*.csv)"));

    if (target.isEmpty()) {
        emit exportFinished(false, QStringLiteral("已取消导出"));
        return QString();
    }

    // content:// URI 以 QString 形式传给 QFile 即可触发 Android content file engine；
    // 本地 file:// URI 则取本地路径。两者统一到 QString。
    const QString filePath = target.isLocalFile() ? target.toLocalFile() : target.toString();
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString msg = QStringLiteral("无法写入所选位置：%1").arg(file.errorString());
        emit exportFinished(false, msg);
        return msg;
    }
    const qint64 written = file.write(csv);
    file.flush();
    file.close();
    if (written != csv.size()) {
        const QString msg = QStringLiteral("写入不完整，请检查存储空间");
        emit exportFinished(false, msg);
        return msg;
    }

    const QString msg = QStringLiteral("已导出 %1 条账目").arg(items.size());
    emit exportFinished(true, msg);
    return QString();
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
    const QVector<Transaction> items = m_db->queryByMonthFiltered(
        m_year, m_month, m_filterType, QString(), m_searchKeyword, &error);
    if (!error.isEmpty())
        emit errorOccurred(error);
    m_model->setTransactions(items);

    const MonthSummary summary = m_db->monthSummary(m_year, m_month, &error);
    m_incomeCents = summary.incomeCents;
    m_expenseCents = summary.expenseCents;
    emit summaryChanged();
}

} // namespace zl
