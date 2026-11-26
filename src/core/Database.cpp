#include "Database.h"
#include "Category.h"

#include <QDir>
#include <QMap>
#include <QFileInfo>
#include <QDateTime>
#include <QStringList>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QObject>

namespace zl {
namespace {

constexpr const char *kConnectionPrefix = "zlexpense_db_";

QString lastError(const QSqlDatabase &db, const QString &fallback)
{
    const QSqlError e = db.lastError();
    return e.type() == QSqlError::NoError ? fallback : e.text();
}

QString lastError(const QSqlQuery &q, const QString &fallback)
{
    const QSqlError e = q.lastError();
    return e.type() == QSqlError::NoError ? fallback : e.text();
}

// 转义 LIKE 通配符，配合 SQL 中的 ESCAPE '\' 使用
QString escapeLike(const QString &raw)
{
    QString out;
    out.reserve(raw.size() + 8);
    for (const QChar ch : raw) {
        if (ch == QLatin1Char('\\') || ch == QLatin1Char('%') || ch == QLatin1Char('_'))
            out.append(QLatin1Char('\\'));
        out.append(ch);
    }
    return out;
}

} // namespace

class Database::Private {
public:
    QString dbPath;
    QString connectionName;

    QSqlDatabase database() const
    {
        return QSqlDatabase::database(connectionName, false);
    }

    bool begin(QSqlDatabase *db, QString *error)
    {
        if (!db->transaction()) {
            if (error)
                *error = lastError(*db, QStringLiteral("无法开启事务"));
            return false;
        }
        return true;
    }
};

Database::Database(const QString &dbPath)
    : d(new Private())
{
    static int sequence = 0;
    d->dbPath = dbPath;
    // 每个实例使用独立连接名，避免多账本/测试间相互干扰
    d->connectionName = QStringLiteral("%1%2_%3")
        .arg(QString::fromLatin1(kConnectionPrefix))
        .arg(QRandomGenerator64::global()->generate())
        .arg(sequence++);
}

Database::~Database()
{
    {
        QSqlDatabase db = QSqlDatabase::database(d->connectionName, false);
        if (db.isValid() && db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(d->connectionName);
    delete d;
}

QString Database::path() const
{
    return d->dbPath;
}

bool Database::isOpen() const
{
    QSqlDatabase db = d->database();
    return db.isValid() && db.isOpen();
}

bool Database::open(QString *error)
{
    if (isOpen())
        return true;

    const QFileInfo fi(d->dbPath);
    if (!fi.absoluteDir().exists() && !QDir().mkpath(fi.absolutePath())) {
        if (error)
            *error = QStringLiteral("无法创建数据库目录：%1").arg(fi.absolutePath());
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), d->connectionName);
    db.setDatabaseName(d->dbPath);
    if (!db.open()) {
        if (error)
            *error = lastError(db, QStringLiteral("无法打开数据库"));
        return false;
    }

    // 建表（幂等）。QSqlQuery::exec 一次只能执行一条语句，需逐条执行。
    const QStringList ddlStatements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS transactions ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  type INTEGER NOT NULL,"
            "  amount_cents INTEGER NOT NULL,"
            "  category TEXT NOT NULL,"
            "  note TEXT NOT NULL DEFAULT '',"
            "  created_ms INTEGER NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_transactions_created_ms "
            "ON transactions(created_ms)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS categories ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  type INTEGER NOT NULL,"
            "  name TEXT NOT NULL,"
            "  builtin INTEGER NOT NULL DEFAULT 0,"
            "  UNIQUE(type, name)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS budgets ("
            "  year INTEGER NOT NULL,"
            "  month INTEGER NOT NULL,"
            "  type INTEGER NOT NULL,"
            "  amount_cents INTEGER NOT NULL,"
            "  PRIMARY KEY(year, month, type)"
            ")")
    };
    for (const QString &sql : ddlStatements) {
        QSqlQuery q(db);
        if (!q.exec(sql)) {
            if (error)
                *error = lastError(q, QStringLiteral("建表失败"));
            db.close();
            return false;
        }
    }

    // 播种内置分类（幂等）：仅在 categories 表为空时写入，避免重复或覆盖自定义数据。
    QSqlQuery cnt(db);
    if (!cnt.exec(QStringLiteral("SELECT COUNT(*) FROM categories"))) {
        if (error) *error = lastError(cnt, QStringLiteral("分类计数失败"));
        return false;
    }
    cnt.next();
    if (cnt.value(0).toInt() == 0) {
        if (!seedBuiltinCategories(db, error))
            return false;
    }
    return true;
}

bool Database::seedBuiltinCategories(QSqlDatabase &db, QString *error)
{
    if (!db.transaction()) {
        if (error) *error = lastError(db, QStringLiteral("无法开启分类播种事务"));
        return false;
    }
    const auto insertOne = [&db, error](int type, const QString &name) -> bool {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT INTO categories(type, name, builtin) VALUES(?, ?, 1)"));
        q.addBindValue(type);
        q.addBindValue(name);
        if (!q.exec()) {
            if (error) *error = lastError(q, QStringLiteral("播种内置分类失败"));
            return false;
        }
        return true;
    };
    const int expenseType = static_cast<int>(TxType::Expense);
    const int incomeType = static_cast<int>(TxType::Income);
    for (const QString &name : Category::expenseNames()) {
        if (!insertOne(expenseType, name)) { db.rollback(); return false; }
    }
    for (const QString &name : Category::incomeNames()) {
        if (!insertOne(incomeType, name)) { db.rollback(); return false; }
    }
    if (!db.commit()) {
        db.rollback();
        if (error) *error = lastError(db, QStringLiteral("提交内置分类失败"));
        return false;
    }
    return true;
}

bool Database::insert(Transaction *tx, QString *error)
{
    if (!tx) {
        if (error) *error = QStringLiteral("空账目");
        return false;
    }
    if (!isValidTransaction(*tx)) {
        if (error) *error = QStringLiteral("账目数据不合法");
        return false;
    }

    QSqlDatabase db = d->database();
    if (!db.isOpen() && !open(error))
        return false;
    if (!d->begin(&db, error))
        return false;

    const std::int64_t createdMs = tx->createdMs > 0
        ? tx->createdMs
        : QDateTime::currentMSecsSinceEpoch();
    const int typeValue = static_cast<int>(tx->type);

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO transactions(type, amount_cents, category, note, created_ms) "
        "VALUES(?, ?, ?, ?, ?)"));
    q.addBindValue(typeValue);
    q.addBindValue(QVariant::fromValue(tx->amountCents));
    q.addBindValue(tx->category);
    q.addBindValue(tx->note);
    q.addBindValue(QVariant::fromValue(createdMs));

    if (!q.exec()) {
        db.rollback();
        if (error) *error = lastError(q, QStringLiteral("插入失败"));
        return false;
    }
    if (!db.commit()) {
        db.rollback();
        if (error) *error = lastError(db, QStringLiteral("提交失败"));
        return false;
    }

    tx->id = q.lastInsertId().toLongLong();
    tx->createdMs = createdMs;
    return true;
}

bool Database::update(const Transaction &tx, QString *error)
{
    if (!isValidTransaction(tx) || tx.id <= 0) {
        if (error) *error = QStringLiteral("账目数据不合法");
        return false;
    }

    QSqlDatabase db = d->database();
    if (!db.isOpen() && !open(error))
        return false;
    if (!d->begin(&db, error))
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE transactions SET type=?, amount_cents=?, category=?, note=?, created_ms=? "
        "WHERE id=?"));
    q.addBindValue(static_cast<int>(tx.type));
    q.addBindValue(QVariant::fromValue(tx.amountCents));
    q.addBindValue(tx.category);
    q.addBindValue(tx.note);
    q.addBindValue(QVariant::fromValue(tx.createdMs));
    q.addBindValue(QVariant::fromValue(tx.id));

    if (!q.exec()) {
        db.rollback();
        if (error) *error = lastError(q, QStringLiteral("更新失败"));
        return false;
    }
    if (q.numRowsAffected() != 1) {
        db.rollback();
        if (error) *error = QStringLiteral("未找到要更新的账目");
        return false;
    }
    if (!db.commit()) {
        db.rollback();
        if (error) *error = lastError(db, QStringLiteral("提交失败"));
        return false;
    }
    return true;
}

bool Database::remove(std::int64_t id, QString *error)
{
    QSqlDatabase db = d->database();
    if (!db.isOpen() && !open(error))
        return false;
    if (!d->begin(&db, error))
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM transactions WHERE id=?"));
    q.addBindValue(QVariant::fromValue(id));
    if (!q.exec()) {
        db.rollback();
        if (error) *error = lastError(q, QStringLiteral("删除失败"));
        return false;
    }
    if (q.numRowsAffected() != 1) {
        db.rollback();
        if (error) *error = QStringLiteral("未找到要删除的账目");
        return false;
    }
    if (!db.commit()) {
        db.rollback();
        if (error) *error = lastError(db, QStringLiteral("提交失败"));
        return false;
    }
    return true;
}

bool Database::get(std::int64_t id, Transaction *out, QString *error) const
{
    if (!out) {
        if (error) *error = QStringLiteral("空输出参数");
        return false;
    }
    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return false;
    }
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, type, amount_cents, category, note, created_ms FROM transactions WHERE id=?"));
    q.addBindValue(QVariant::fromValue(id));
    if (!q.exec()) {
        if (error) *error = lastError(q, QStringLiteral("查询失败"));
        return false;
    }
    if (!q.next()) {
        if (error) *error = QStringLiteral("未找到账目");
        return false;
    }
    out->id = q.value(0).toLongLong();
    out->type = static_cast<TxType>(q.value(1).toInt());
    out->amountCents = q.value(2).toLongLong();
    out->category = q.value(3).toString();
    out->note = q.value(4).toString();
    out->createdMs = q.value(5).toLongLong();
    return true;
}

QVector<Transaction> Database::queryByMonth(int year, int month, QString *error) const
{
    return queryByMonthFiltered(year, month, -1, QString(), QString(), error);
}

QVector<Transaction> Database::queryByMonthFiltered(int year, int month, int type,
                                                    const QString &category,
                                                    const QString &keyword,
                                                    QString *error) const
{
    QVector<Transaction> result;
    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return result;
    }

    const QDate start(year, month, 1);
    if (!start.isValid()) {
        if (error) *error = QStringLiteral("无效的年月");
        return result;
    }
    const QDate end = start.addMonths(1);
    const std::int64_t startMs = QDateTime(start, QTime(0, 0, 0)).toMSecsSinceEpoch();
    const std::int64_t endMs = QDateTime(end, QTime(0, 0, 0)).toMSecsSinceEpoch();

    QString sql = QStringLiteral(
        "SELECT id, type, amount_cents, category, note, created_ms "
        "FROM transactions WHERE created_ms >= ? AND created_ms < ?");
    QVariantList binds;
    binds << QVariant::fromValue(startMs) << QVariant::fromValue(endMs);

    if (type == static_cast<int>(TxType::Expense) || type == static_cast<int>(TxType::Income)) {
        sql += QStringLiteral(" AND type = ?");
        binds << type;
    }

    const QString cat = category.trimmed();
    if (!cat.isEmpty()) {
        sql += QStringLiteral(" AND category = ?");
        binds << cat;
    }

    const QString kw = keyword.trimmed();
    if (!kw.isEmpty()) {
        sql += QStringLiteral(
            " AND (category LIKE ? ESCAPE '\\' OR note LIKE ? ESCAPE '\\')");
        const QString pattern = QStringLiteral("%") + escapeLike(kw) + QStringLiteral("%");
        binds << pattern << pattern;
    }

    sql += QStringLiteral(" ORDER BY created_ms DESC, id DESC");

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &v : binds)
        q.addBindValue(v);
    if (!q.exec()) {
        if (error) *error = lastError(q, QStringLiteral("查询失败"));
        return result;
    }
    while (q.next()) {
        Transaction tx;
        tx.id = q.value(0).toLongLong();
        tx.type = static_cast<TxType>(q.value(1).toInt());
        tx.amountCents = q.value(2).toLongLong();
        tx.category = q.value(3).toString();
        tx.note = q.value(4).toString();
        tx.createdMs = q.value(5).toLongLong();
        result.append(tx);
    }
    return result;
}

MonthSummary Database::monthSummary(int year, int month, QString *error) const
{
    MonthSummary summary;
    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return summary;
    }

    const QDate start(year, month, 1);
    if (!start.isValid()) {
        if (error) *error = QStringLiteral("无效的年月");
        return summary;
    }
    const QDate end = start.addMonths(1);
    const std::int64_t startMs = QDateTime(start, QTime(0, 0, 0)).toMSecsSinceEpoch();
    const std::int64_t endMs = QDateTime(end, QTime(0, 0, 0)).toMSecsSinceEpoch();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT type, SUM(amount_cents), COUNT(*) FROM transactions "
        "WHERE created_ms >= ? AND created_ms < ? GROUP BY type"));
    q.addBindValue(QVariant::fromValue(startMs));
    q.addBindValue(QVariant::fromValue(endMs));
    if (!q.exec()) {
        if (error) *error = lastError(q, QStringLiteral("汇总失败"));
        return summary;
    }
    while (q.next()) {
        const int type = q.value(0).toInt();
        const std::int64_t sum = q.value(1).toLongLong();
        const int cnt = q.value(2).toInt();
        summary.count += cnt;
        if (type == static_cast<int>(TxType::Income))
            summary.incomeCents += sum;
        else
            summary.expenseCents += sum;
    }
    return summary;
}

QVector<CategoryStat> Database::monthCategoryStats(int year, int month, int type,
                                                  QString *error) const
{
    QVector<CategoryStat> result;
    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return result;
    }
    if (type != static_cast<int>(TxType::Expense) && type != static_cast<int>(TxType::Income)) {
        if (error) *error = QStringLiteral("无效的收支类型");
        return result;
    }

    const QDate start(year, month, 1);
    if (!start.isValid()) {
        if (error) *error = QStringLiteral("无效的年月");
        return result;
    }
    const QDate end = start.addMonths(1);
    const std::int64_t startMs = QDateTime(start, QTime(0, 0, 0)).toMSecsSinceEpoch();
    const std::int64_t endMs = QDateTime(end, QTime(0, 0, 0)).toMSecsSinceEpoch();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT category, SUM(amount_cents), COUNT(*) FROM transactions "
        "WHERE created_ms >= ? AND created_ms < ? AND type = ? "
        "GROUP BY category ORDER BY SUM(amount_cents) DESC, category ASC"));
    q.addBindValue(QVariant::fromValue(startMs));
    q.addBindValue(QVariant::fromValue(endMs));
    q.addBindValue(type);
    if (!q.exec()) {
        if (error) *error = lastError(q, QStringLiteral("分类汇总失败"));
        return result;
    }
    while (q.next()) {
        CategoryStat stat;
        stat.category = q.value(0).toString();
        stat.totalCents = q.value(1).toLongLong();
        stat.count = q.value(2).toInt();
        result.append(stat);
    }
    return result;
}

int Database::count(QString *error) const
{
    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return 0;
    }
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM transactions"))) {
        if (error) *error = lastError(q, QStringLiteral("计数失败"));
        return 0;
    }
    if (q.next())
        return q.value(0).toInt();
    return 0;
}

QVector<CategoryRecord> Database::categories(int type, QString *error) const
{
    QVector<CategoryRecord> result;
    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return result;
    }
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT name, type, builtin FROM categories WHERE type=? "
        "ORDER BY builtin DESC, id ASC"));
    q.addBindValue(type);
    if (!q.exec()) {
        if (error) *error = lastError(q, QStringLiteral("查询分类失败"));
        return result;
    }
    while (q.next()) {
        CategoryRecord c;
        c.name = q.value(0).toString();
        c.type = q.value(1).toInt();
        c.builtin = q.value(2).toBool();
        result.append(c);
    }
    return result;
}

bool Database::addCategory(int type, const QString &name, QString *error)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (error) *error = QStringLiteral("分类名称不能为空");
        return false;
    }

    QSqlDatabase db = d->database();
    if (!db.isOpen() && !open(error))
        return false;
    if (!d->begin(&db, error))
        return false;

    QSqlQuery chk(db);
    chk.prepare(QStringLiteral("SELECT COUNT(*) FROM categories WHERE type=? AND name=?"));
    chk.addBindValue(type);
    chk.addBindValue(trimmed);
    if (!chk.exec() || !chk.next()) {
        db.rollback();
        if (error) *error = lastError(chk, QStringLiteral("检查分类失败"));
        return false;
    }
    if (chk.value(0).toInt() > 0) {
        db.rollback();
        if (error) *error = QStringLiteral("分类已存在");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO categories(type, name, builtin) VALUES(?, ?, 0)"));
    q.addBindValue(type);
    q.addBindValue(trimmed);
    if (!q.exec()) {
        db.rollback();
        if (error) *error = lastError(q, QStringLiteral("新增分类失败"));
        return false;
    }
    if (!db.commit()) {
        db.rollback();
        if (error) *error = lastError(db, QStringLiteral("提交失败"));
        return false;
    }
    return true;
}

bool Database::removeCategory(int type, const QString &name, QString *error)
{
    QSqlDatabase db = d->database();
    if (!db.isOpen() && !open(error))
        return false;

    // 先只读校验：内置不可删；被账目引用不可删（防止历史账目分类丢失）。
    QSqlQuery chk(db);
    chk.prepare(QStringLiteral("SELECT builtin FROM categories WHERE type=? AND name=?"));
    chk.addBindValue(type);
    chk.addBindValue(name);
    if (!chk.exec() || !chk.next()) {
        if (error) *error = QStringLiteral("分类不存在");
        return false;
    }
    if (chk.value(0).toBool()) {
        if (error) *error = QStringLiteral("内置分类不可删除");
        return false;
    }

    QSqlQuery used(db);
    used.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM transactions WHERE type=? AND category=?"));
    used.addBindValue(type);
    used.addBindValue(name);
    if (!used.exec() || !used.next()) {
        if (error) *error = lastError(used, QStringLiteral("检查分类引用失败"));
        return false;
    }
    if (used.value(0).toInt() > 0) {
        if (error) *error = QStringLiteral("该分类已被账目使用，不可删除");
        return false;
    }

    if (!d->begin(&db, error))
        return false;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM categories WHERE type=? AND name=? AND builtin=0"));
    q.addBindValue(type);
    q.addBindValue(name);
    if (!q.exec()) {
        db.rollback();
        if (error) *error = lastError(q, QStringLiteral("删除分类失败"));
        return false;
    }
    if (q.numRowsAffected() != 1) {
        db.rollback();
        if (error) *error = QStringLiteral("未找到要删除的自定义分类");
        return false;
    }
    if (!db.commit()) {
        db.rollback();
        if (error) *error = lastError(db, QStringLiteral("提交失败"));
        return false;
    }
    return true;
}

bool Database::setBudget(int year, int month, int type, std::int64_t amountCents,
                         QString *error)
{
    if (year < 2000 || month < 1 || month > 12 || amountCents < 0) {
        if (error) *error = QStringLiteral("预算参数无效");
        return false;
    }

    QSqlDatabase db = d->database();
    if (!db.isOpen() && !open(error))
        return false;
    if (!d->begin(&db, error))
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO budgets(year, month, type, amount_cents) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(year, month, type) DO UPDATE SET amount_cents=excluded.amount_cents"));
    q.addBindValue(year);
    q.addBindValue(month);
    q.addBindValue(type);
    q.addBindValue(QVariant::fromValue(amountCents));
    if (!q.exec()) {
        db.rollback();
        if (error) *error = lastError(q, QStringLiteral("设置预算失败"));
        return false;
    }
    if (!db.commit()) {
        db.rollback();
        if (error) *error = lastError(db, QStringLiteral("提交预算失败"));
        return false;
    }
    return true;
}

std::int64_t Database::budget(int year, int month, int type, bool *found, QString *error) const
{
    if (found) *found = false;
    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return 0;
    }
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT amount_cents FROM budgets WHERE year=? AND month=? AND type=?"));
    q.addBindValue(year);
    q.addBindValue(month);
    q.addBindValue(type);
    if (!q.exec()) {
        if (error) *error = lastError(q, QStringLiteral("查询预算失败"));
        return 0;
    }
    if (q.next()) {
        if (found) *found = true;
        return q.value(0).toLongLong();
    }
    return 0;
}

bool Database::removeBudget(int year, int month, int type, QString *error)
{
    QSqlDatabase db = d->database();
    if (!db.isOpen() && !open(error))
        return false;
    if (!d->begin(&db, error))
        return false;
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "DELETE FROM budgets WHERE year=? AND month=? AND type=?"));
    q.addBindValue(year);
    q.addBindValue(month);
    q.addBindValue(type);
    if (!q.exec()) {
        db.rollback();
        if (error) *error = lastError(q, QStringLiteral("删除预算失败"));
        return false;
    }
    if (!db.commit()) {
        db.rollback();
        if (error) *error = lastError(db, QStringLiteral("提交失败"));
        return false;
    }
    return true;
}

QVector<MonthPoint> Database::monthSeries(int year, int month, int count,
                                          QString *error) const
{
    QVector<MonthPoint> result;
    if (count <= 0)
        return result;

    // 构造从 (year, month) 往前 count-1 个月的升序序列
    QVector<QPair<int, int>> months;
    int y = year;
    int m = month;
    for (int i = 0; i < count; ++i) {
        months.prepend(qMakePair(y, m));
        m -= 1;
        if (m < 1) { m = 12; y -= 1; }
    }

    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return result;
    }

    // 月初/月末用本地时间毫秒，与 queryByMonth 保持一致
    const auto boundsOf = [](int yy, int mm, qint64 *from, qint64 *to) {
        const QDate first(yy, mm, 1);
        const QDate next = (mm == 12) ? QDate(yy + 1, 1, 1) : QDate(yy, mm + 1, 1);
        *from = QDateTime(first, QTime(0, 0, 0)).toMSecsSinceEpoch();
        *to = QDateTime(next, QTime(0, 0, 0)).toMSecsSinceEpoch();
    };

    for (const auto &ym : months) {
        qint64 from = 0, to = 0;
        boundsOf(ym.first, ym.second, &from, &to);
        MonthPoint point;
        point.year = ym.first;
        point.month = ym.second;

        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT type, COALESCE(SUM(amount_cents), 0) FROM transactions "
            "WHERE created_ms >= ? AND created_ms < ? GROUP BY type"));
        q.addBindValue(QVariant::fromValue(from));
        q.addBindValue(QVariant::fromValue(to));
        if (!q.exec()) {
            if (error) *error = lastError(q, QStringLiteral("查询月度趋势失败"));
            return {};
        }
        while (q.next()) {
            const int t = q.value(0).toInt();
            const qint64 sum = q.value(1).toLongLong();
            if (t == static_cast<int>(TxType::Income)) point.incomeCents = sum;
            else point.expenseCents = sum;
        }
        result.append(point);
    }
    return result;
}

QVector<DaySummary> Database::monthDailySummary(int year, int month, QString *error) const
{
    QVector<DaySummary> result;
    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return result;
    }

    // 复用月度查询，再按本地日期在 C++ 分组：避免 SQLite 时区换算陷阱，逻辑清晰。
    const QVector<Transaction> items = queryByMonth(year, month, error);
    QMap<int, DaySummary> byDay;
    for (const Transaction &tx : items) {
        const int day = QDateTime::fromMSecsSinceEpoch(tx.createdMs).date().day();
        DaySummary &d = byDay[day];
        d.day = day;
        if (tx.type == TxType::Income) d.incomeCents += tx.amountCents;
        else d.expenseCents += tx.amountCents;
        d.count += 1;
    }
    for (auto it = byDay.constBegin(); it != byDay.constEnd(); ++it)
        result.append(it.value());
    return result;
}

QVector<Transaction> Database::queryByDate(int year, int month, int day,
                                           QString *error) const
{
    QVector<Transaction> result;
    if (day < 1 || day > QDate(year, month, 1).daysInMonth()) {
        if (error) *error = QStringLiteral("日期无效");
        return result;
    }
    const qint64 from = QDateTime(QDate(year, month, day), QTime(0, 0, 0)).toMSecsSinceEpoch();
    const QDate nextDate = QDate(year, month, day).addDays(1);
    const qint64 to = QDateTime(nextDate, QTime(0, 0, 0)).toMSecsSinceEpoch();

    QSqlDatabase db = d->database();
    if (!db.isOpen()) {
        if (error) *error = QStringLiteral("数据库未打开");
        return result;
    }
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, type, amount_cents, category, note, created_ms "
        "FROM transactions WHERE created_ms >= ? AND created_ms < ? "
        "ORDER BY created_ms DESC, id DESC"));
    q.addBindValue(QVariant::fromValue(from));
    q.addBindValue(QVariant::fromValue(to));
    if (!q.exec()) {
        if (error) *error = lastError(q, QStringLiteral("查询当日账目失败"));
        return {};
    }
    while (q.next()) {
        Transaction tx;
        tx.id = q.value(0).toLongLong();
        tx.type = static_cast<TxType>(q.value(1).toInt());
        tx.amountCents = q.value(2).toLongLong();
        tx.category = q.value(3).toString();
        tx.note = q.value(4).toString();
        tx.createdMs = q.value(5).toLongLong();
        result.append(tx);
    }
    return result;
}

} // namespace zl
