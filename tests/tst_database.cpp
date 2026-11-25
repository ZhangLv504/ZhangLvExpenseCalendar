#include <QtTest>
#include <QTemporaryDir>
#include <QDate>
#include <QDateTime>

#include "Database.h"
#include "Money.h"

class DatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void openCreatesDb();
    void insertAndGet();
    void updateRecord();
    void deleteRecord();
    void queryByMonthBoundary();   // 跨月边界
    void monthSummary();           // 月度收支汇总
    void emptyDatabase();          // 空账本
    void transactionRollback();    // 事务：非法数据不落库
    void largeAmount();            // 边界金额（int64 分）
    void categorySeed();           // 内置分类播种
    void categoryCustomCrud();     // 自定义分类增删与重名拒绝
    void categoryDeleteGuards();   // 内置/被引用分类不可删

private:
    QTemporaryDir m_dir;
    zl::Database *m_db = nullptr;

    zl::Transaction makeTx(zl::TxType type, qint64 cents,
                           const QString &category, qint64 createdMs) const;
};

void DatabaseTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new zl::Database(m_dir.filePath(QStringLiteral("ledger.sqlite")));
}

void DatabaseTest::cleanupTestCase()
{
    delete m_db;
    m_db = nullptr;
}

zl::Transaction DatabaseTest::makeTx(zl::TxType type, qint64 cents,
                                     const QString &category, qint64 createdMs) const
{
    zl::Transaction tx;
    tx.type = type;
    tx.amountCents = cents;
    tx.category = category;
    tx.note = QStringLiteral("测试备注");
    if (createdMs > 0)
        tx.createdMs = createdMs;
    return tx;
}

void DatabaseTest::openCreatesDb()
{
    QString err;
    QVERIFY2(m_db->open(&err), qPrintable(err));
    QVERIFY(m_db->isOpen());
    QVERIFY(QFile::exists(m_db->path()));
    QCOMPARE(m_db->count(), 0);
    // 再次 open 幂等
    QVERIFY(m_db->open(&err));
}

void DatabaseTest::insertAndGet()
{
    qint64 ts = QDateTime(QDate(2026, 11, 15), QTime(12, 0, 0)).toMSecsSinceEpoch();
    zl::Transaction tx = makeTx(zl::TxType::Expense, 1234, QStringLiteral("餐饮"), ts);
    QString err;
    QVERIFY2(m_db->insert(&tx, &err), qPrintable(err));
    QVERIFY(tx.id > 0);
    QCOMPARE(tx.createdMs, ts);

    zl::Transaction got;
    QVERIFY2(m_db->get(tx.id, &got, &err), qPrintable(err));
    QCOMPARE(got.id, tx.id);
    QCOMPARE(got.type, zl::TxType::Expense);
    QCOMPARE(got.amountCents, qint64(1234));
    QCOMPARE(got.category, QStringLiteral("餐饮"));
    QCOMPARE(got.note, QStringLiteral("测试备注"));
    QCOMPARE(got.createdMs, ts);
}

void DatabaseTest::updateRecord()
{
    qint64 ts = QDateTime(QDate(2026, 11, 16), QTime(9, 30, 0)).toMSecsSinceEpoch();
    zl::Transaction tx = makeTx(zl::TxType::Income, 500000, QStringLiteral("工资"), ts);
    QString err;
    QVERIFY(m_db->insert(&tx, &err));

    tx.amountCents = 600000;
    tx.category = QStringLiteral("奖金");
    tx.note = QStringLiteral("更新后");
    QVERIFY2(m_db->update(tx, &err), qPrintable(err));

    zl::Transaction got;
    QVERIFY(m_db->get(tx.id, &got, &err));
    QCOMPARE(got.amountCents, qint64(600000));
    QCOMPARE(got.category, QStringLiteral("奖金"));
    QCOMPARE(got.note, QStringLiteral("更新后"));
}

void DatabaseTest::deleteRecord()
{
    qint64 ts = QDateTime(QDate(2026, 11, 17), QTime(8, 0, 0)).toMSecsSinceEpoch();
    zl::Transaction tx = makeTx(zl::TxType::Expense, 100, QStringLiteral("交通"), ts);
    QString err;
    QVERIFY(m_db->insert(&tx, &err));
    QVERIFY2(m_db->remove(tx.id, &err), qPrintable(err));

    zl::Transaction got;
    QVERIFY(!m_db->get(tx.id, &got, &err));
    // 删除不存在的 id 应失败
    QVERIFY(!m_db->remove(tx.id, &err));
}

void DatabaseTest::queryByMonthBoundary()
{
    // 使用独立数据库，避免与其他测试用例的写入相互污染
    zl::Database db(m_dir.filePath(QStringLiteral("boundary.sqlite")));
    QString err;
    QVERIFY(db.open(&err));

    // 10月31日 23:59、11月1日 00:00、11月30日 23:59、12月1日 00:00
    const qint64 tOctEnd = QDateTime(QDate(2026, 10, 31), QTime(23, 59, 59)).toMSecsSinceEpoch();
    const qint64 tNovStart = QDateTime(QDate(2026, 11, 1), QTime(0, 0, 0)).toMSecsSinceEpoch();
    const qint64 tNovEnd = QDateTime(QDate(2026, 11, 30), QTime(23, 59, 59)).toMSecsSinceEpoch();
    const qint64 tDecStart = QDateTime(QDate(2026, 12, 1), QTime(0, 0, 0)).toMSecsSinceEpoch();

    zl::Transaction a = makeTx(zl::TxType::Expense, 100, QStringLiteral("A"), tOctEnd);
    zl::Transaction b = makeTx(zl::TxType::Expense, 200, QStringLiteral("B"), tNovStart);
    zl::Transaction c = makeTx(zl::TxType::Expense, 300, QStringLiteral("C"), tNovEnd);
    zl::Transaction d = makeTx(zl::TxType::Expense, 400, QStringLiteral("D"), tDecStart);
    QVERIFY(db.insert(&a, &err));
    QVERIFY(db.insert(&b, &err));
    QVERIFY(db.insert(&c, &err));
    QVERIFY(db.insert(&d, &err));

    const QVector<zl::Transaction> nov = db.queryByMonth(2026, 11, &err);
    QCOMPARE(nov.size(), 2);
    // 时间倒序：C 在 B 前
    QCOMPARE(nov.at(0).category, QStringLiteral("C"));
    QCOMPARE(nov.at(1).category, QStringLiteral("B"));
}

void DatabaseTest::monthSummary()
{
    QString err;
    const qint64 t1 = QDateTime(QDate(2026, 9, 10), QTime(10, 0, 0)).toMSecsSinceEpoch();
    const qint64 t2 = QDateTime(QDate(2026, 9, 20), QTime(10, 0, 0)).toMSecsSinceEpoch();
    zl::Transaction income = makeTx(zl::TxType::Income, 1000000, QStringLiteral("工资"), t1);
    zl::Transaction expense = makeTx(zl::TxType::Expense, 2550, QStringLiteral("餐饮"), t2);
    QVERIFY(m_db->insert(&income, &err));
    QVERIFY(m_db->insert(&expense, &err));

    const zl::MonthSummary s = m_db->monthSummary(2026, 9, &err);
    QCOMPARE(s.incomeCents, qint64(1000000));
    QCOMPARE(s.expenseCents, qint64(2550));
    QCOMPARE(s.count, 2);

    // 无数据月份
    const zl::MonthSummary empty = m_db->monthSummary(2020, 1, &err);
    QCOMPARE(empty.incomeCents, qint64(0));
    QCOMPARE(empty.expenseCents, qint64(0));
    QCOMPARE(empty.count, 0);
}

void DatabaseTest::emptyDatabase()
{
    // 独立空库，验证空账本查询不报错
    zl::Database emptyDb(m_dir.filePath(QStringLiteral("empty.sqlite")));
    QString err;
    QVERIFY(emptyDb.open(&err));
    QCOMPARE(emptyDb.count(&err), 0);
    QVERIFY(emptyDb.queryByMonth(2026, 11, &err).isEmpty());
}

void DatabaseTest::transactionRollback()
{
    QString err;
    const int before = m_db->count(&err);
    // 非法账目（分类为空）应被拒绝，且不落库
    zl::Transaction bad = makeTx(zl::TxType::Expense, 100, QString(), 0);
    QVERIFY(!m_db->insert(&bad, &err));
    QCOMPARE(m_db->count(&err), before);
}

void DatabaseTest::largeAmount()
{
    QString err;
    // 亿元级金额，int64 分不能有精度损失
    zl::Transaction tx = makeTx(zl::TxType::Income, qint64(999999999999), QStringLiteral("理财"), 0);
    QVERIFY2(m_db->insert(&tx, &err), qPrintable(err));
    zl::Transaction got;
    QVERIFY(m_db->get(tx.id, &got, &err));
    QCOMPARE(got.amountCents, qint64(999999999999));
    QCOMPARE(zl::Money::format(got.amountCents), QStringLiteral("9999999999.99"));
}

void DatabaseTest::categorySeed()
{
    QString err;
    const QVector<zl::CategoryRecord> expenses = m_db->categories(
        static_cast<int>(zl::TxType::Expense), &err);
    QVERIFY2(expenses.size() >= 9, qPrintable(err));
    QVERIFY(expenses.first().builtin);
    QVERIFY(expenses.first().name == QStringLiteral("餐饮"));

    const QVector<zl::CategoryRecord> incomes = m_db->categories(
        static_cast<int>(zl::TxType::Income), &err);
    QVERIFY(incomes.size() >= 6);
    QVERIFY(incomes.first().builtin);
}

void DatabaseTest::categoryCustomCrud()
{
    QString err;
    const int expenseType = static_cast<int>(zl::TxType::Expense);
    QVERIFY(m_db->addCategory(expenseType, QStringLiteral("  宠物  "), &err));
    const QVector<zl::CategoryRecord> list = m_db->categories(expenseType, &err);
    bool found = false;
    for (const zl::CategoryRecord &c : list) {
        if (c.name == QStringLiteral("宠物")) {
            found = true;
            QVERIFY(!c.builtin);
        }
    }
    QVERIFY(found);

    // 重名（含内置）拒绝
    QVERIFY(!m_db->addCategory(expenseType, QStringLiteral("宠物"), &err));
    QVERIFY(!m_db->addCategory(expenseType, QStringLiteral("餐饮"), &err));
    // 空名拒绝
    QVERIFY(!m_db->addCategory(expenseType, QStringLiteral("   "), &err));
    // 跨类型同名允许
    QVERIFY(m_db->addCategory(static_cast<int>(zl::TxType::Income),
                              QStringLiteral("宠物"), &err));

    QVERIFY(m_db->removeCategory(expenseType, QStringLiteral("宠物"), &err));
    QVERIFY(!m_db->removeCategory(expenseType, QStringLiteral("宠物"), &err));
}

void DatabaseTest::categoryDeleteGuards()
{
    QString err;
    const int expenseType = static_cast<int>(zl::TxType::Expense);
    // 内置分类不可删
    QVERIFY(!m_db->removeCategory(expenseType, QStringLiteral("餐饮"), &err));

    QVERIFY(m_db->addCategory(expenseType, QStringLiteral("旅行"), &err));
    // 创建一笔使用该分类的账目
    zl::Transaction tx = makeTx(zl::TxType::Expense, 1000, QStringLiteral("旅行"), 0);
    QVERIFY(m_db->insert(&tx, &err));
    // 已被账目引用，不可删除
    QVERIFY(!m_db->removeCategory(expenseType, QStringLiteral("旅行"), &err));

    // 删除账目后即可删除分类
    QVERIFY(m_db->remove(tx.id, &err));
    QVERIFY(m_db->removeCategory(expenseType, QStringLiteral("旅行"), &err));
}

QTEST_MAIN(DatabaseTest)
#include "tst_database.moc"
