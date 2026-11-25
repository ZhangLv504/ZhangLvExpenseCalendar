#include "Database.h"

#include <QDir>
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
            "ON transactions(created_ms)")
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

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, type, amount_cents, category, note, created_ms "
        "FROM transactions WHERE created_ms >= ? AND created_ms < ? "
        "ORDER BY created_ms DESC, id DESC"));
    q.addBindValue(QVariant::fromValue(startMs));
    q.addBindValue(QVariant::fromValue(endMs));
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

} // namespace zl
