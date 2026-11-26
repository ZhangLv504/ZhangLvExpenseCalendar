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
    void filterCombinations();     // 类型/分类/关键字/组合筛选
    void filterKeywordEscapes();   // LIKE 通配符转义
    void monthCategoryStats();     // 月度分类汇总
    void budgetCrud();             // 预算 upsert/读取/删除/跨月隔离
    void monthSeriesTrend();       // 近 N 月趋势归属
    void dailySummaryAndByDate();  // 每日汇总与按日查询

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

void DatabaseTest::filterCombinations()
{
    zl::Database db(m_dir.filePath(QStringLiteral("filter.sqlite")));
    QString err;
    QVERIFY2(db.open(&err), qPrintable(err));

    const qint64 t1 = QDateTime(QDate(2027, 3, 10), QTime(9, 0, 0)).toMSecsSinceEpoch();
    const qint64 t2 = QDateTime(QDate(2027, 3, 20), QTime(18, 30, 0)).toMSecsSinceEpoch();
    const qint64 tOut = QDateTime(QDate(2027, 4, 1), QTime(0, 0, 0)).toMSecsSinceEpoch();

    const auto add = [&db, &err](zl::TxType type, qint64 cents, const QString &category,
                                 const QString &note, qint64 createdMs) {
        zl::Transaction tx;
        tx.type = type;
        tx.amountCents = cents;
        tx.category = category;
        tx.note = note;
        tx.createdMs = createdMs;
        return db.insert(&tx, &err);
    };

    QVERIFY(add(zl::TxType::Expense, 1200, QStringLiteral("餐饮"),
                QStringLiteral("午餐 面条"), t1));
    QVERIFY(add(zl::TxType::Expense, 800, QStringLiteral("交通"),
                QStringLiteral("地铁通勤"), t2));
    QVERIFY(add(zl::TxType::Income, 100000, QStringLiteral("工资"),
                QStringLiteral("三月工资"), t1));
    QVERIFY(add(zl::TxType::Expense, 999, QStringLiteral("餐饮"),
                QStringLiteral("跨月记录"), tOut));

    // 类型筛选：0 支出 / 1 收入 / -1 全部
    QCOMPARE(db.queryByMonthFiltered(2027, 3, 0, QString(), QString(), &err).size(), 2);
    QCOMPARE(db.queryByMonthFiltered(2027, 3, 1, QString(), QString(), &err).size(), 1);
    QCOMPARE(db.queryByMonthFiltered(2027, 3, -1, QString(), QString(), &err).size(), 3);

    // 分类筛选
    const QVector<zl::Transaction> food =
        db.queryByMonthFiltered(2027, 3, -1, QStringLiteral("餐饮"), QString(), &err);
    QCOMPARE(food.size(), 1);
    QCOMPARE(food.first().note, QStringLiteral("午餐 面条"));

    // 类型 + 分类组合
    QCOMPARE(db.queryByMonthFiltered(2027, 3, 1, QStringLiteral("餐饮"), QString(), &err).size(), 0);

    // 关键字命中备注 / 命中分类 / 与类型组合
    QCOMPARE(db.queryByMonthFiltered(2027, 3, -1, QString(),
                                     QStringLiteral("地铁"), &err).size(), 1);
    QCOMPARE(db.queryByMonthFiltered(2027, 3, -1, QString(),
                                     QStringLiteral("工资"), &err).size(), 1);
    QCOMPARE(db.queryByMonthFiltered(2027, 3, 0, QString(),
                                     QStringLiteral("工资"), &err).size(), 0);
    // 空关键字（含纯空格）不过滤
    QCOMPARE(db.queryByMonthFiltered(2027, 3, -1, QString(),
                                     QStringLiteral("   "), &err).size(), 3);
    // 无命中
    QVERIFY(db.queryByMonthFiltered(2027, 3, -1, QString(),
                                    QStringLiteral("不存在"), &err).isEmpty());
}

void DatabaseTest::filterKeywordEscapes()
{
    zl::Database db(m_dir.filePath(QStringLiteral("like.sqlite")));
    QString err;
    QVERIFY2(db.open(&err), qPrintable(err));

    const qint64 t = QDateTime(QDate(2027, 5, 8), QTime(12, 0, 0)).toMSecsSinceEpoch();
    const auto add = [&db, &err, t](const QString &note) {
        zl::Transaction tx;
        tx.type = zl::TxType::Expense;
        tx.amountCents = 100;
        tx.category = QStringLiteral("其他");
        tx.note = note;
        tx.createdMs = t;
        return db.insert(&tx, &err);
    };
    QVERIFY(add(QStringLiteral("折扣 100%")));
    QVERIFY(add(QStringLiteral("折扣 100 元")));
    QVERIFY(add(QStringLiteral("a_b")));
    QVERIFY(add(QStringLiteral("axb")));
    QVERIFY(add(QStringLiteral("反斜杠 \\ 测试")));

    // "%" 只匹配包含字面百分号的记录，不能变成“匹配全部”
    const QVector<zl::Transaction> percent =
        db.queryByMonthFiltered(2027, 5, -1, QString(), QStringLiteral("%"), &err);
    QCOMPARE(percent.size(), 1);
    QCOMPARE(percent.first().note, QStringLiteral("折扣 100%"));

    // "_" 只匹配包含字面下划线的记录
    const QVector<zl::Transaction> underscore =
        db.queryByMonthFiltered(2027, 5, -1, QString(), QStringLiteral("_"), &err);
    QCOMPARE(underscore.size(), 1);
    QCOMPARE(underscore.first().note, QStringLiteral("a_b"));

    // 反斜杠同样按字面匹配
    QCOMPARE(db.queryByMonthFiltered(2027, 5, -1, QString(),
                                     QStringLiteral("\\"), &err).size(), 1);
}

void DatabaseTest::monthCategoryStats()
{
    zl::Database db(m_dir.filePath(QStringLiteral("stats.sqlite")));
    QString err;
    QVERIFY2(db.open(&err), qPrintable(err));

    const qint64 t1 = QDateTime(QDate(2027, 4, 5), QTime(10, 0, 0)).toMSecsSinceEpoch();
    const qint64 t2 = QDateTime(QDate(2027, 4, 25), QTime(20, 0, 0)).toMSecsSinceEpoch();
    const qint64 tMay = QDateTime(QDate(2027, 5, 2), QTime(9, 0, 0)).toMSecsSinceEpoch();

    const auto add = [&db, &err](zl::TxType type, qint64 cents, const QString &category,
                                 qint64 createdMs) {
        zl::Transaction tx;
        tx.type = type;
        tx.amountCents = cents;
        tx.category = category;
        tx.note = QStringLiteral("统计");
        tx.createdMs = createdMs;
        return db.insert(&tx, &err);
    };
    QVERIFY(add(zl::TxType::Expense, 1000, QStringLiteral("餐饮"), t1));
    QVERIFY(add(zl::TxType::Expense, 500, QStringLiteral("餐饮"), t2));
    QVERIFY(add(zl::TxType::Expense, 2000, QStringLiteral("交通"), t1));
    QVERIFY(add(zl::TxType::Income, 99999, QStringLiteral("工资"), t1));
    QVERIFY(add(zl::TxType::Expense, 700, QStringLiteral("餐饮"), tMay));

    const QVector<zl::CategoryStat> expenses =
        db.monthCategoryStats(2027, 4, static_cast<int>(zl::TxType::Expense), &err);
    QCOMPARE(expenses.size(), 2);
    // 金额降序：交通 2000 在餐饮 1500 前；餐饮两笔合并
    QCOMPARE(expenses.at(0).category, QStringLiteral("交通"));
    QCOMPARE(expenses.at(0).totalCents, qint64(2000));
    QCOMPARE(expenses.at(0).count, 1);
    QCOMPARE(expenses.at(1).category, QStringLiteral("餐饮"));
    QCOMPARE(expenses.at(1).totalCents, qint64(1500));
    QCOMPARE(expenses.at(1).count, 2);

    const QVector<zl::CategoryStat> incomes =
        db.monthCategoryStats(2027, 4, static_cast<int>(zl::TxType::Income), &err);
    QCOMPARE(incomes.size(), 1);
    QCOMPARE(incomes.first().category, QStringLiteral("工资"));
    QCOMPARE(incomes.first().totalCents, qint64(99999));

    // 跨月归属：5 月只统计 5 月的 700
    const QVector<zl::CategoryStat> may =
        db.monthCategoryStats(2027, 5, static_cast<int>(zl::TxType::Expense), &err);
    QCOMPARE(may.size(), 1);
    QCOMPARE(may.first().totalCents, qint64(700));

    // 空月份
    QVERIFY(db.monthCategoryStats(2020, 1, static_cast<int>(zl::TxType::Expense), &err).isEmpty());

    // 非法类型
    err.clear();
    QVERIFY(db.monthCategoryStats(2027, 4, 5, &err).isEmpty());
    QVERIFY(!err.isEmpty());
}

void DatabaseTest::budgetCrud()
{
    QString err;
    const int expense = static_cast<int>(zl::TxType::Expense);
    const int income = static_cast<int>(zl::TxType::Income);

    // 初始无预算
    bool found = true;
    QCOMPARE(m_db->budget(2026, 11, expense, &found, &err), qint64(0));
    QVERIFY(!found);

    // 设置预算
    QVERIFY(m_db->setBudget(2026, 11, expense, 300000, &err)); // 3000.00
    QCOMPARE(m_db->budget(2026, 11, expense, &found, &err), qint64(300000));
    QVERIFY(found);

    // upsert 覆盖
    QVERIFY(m_db->setBudget(2026, 11, expense, 500000, &err));
    QCOMPARE(m_db->budget(2026, 11, expense, &found, &err), qint64(500000));

    // 跨类型/跨月隔离
    QVERIFY(m_db->setBudget(2026, 11, income, 1000, &err));
    QVERIFY(m_db->setBudget(2026, 12, expense, 2000, &err));
    QCOMPARE(m_db->budget(2026, 11, income, &found, &err), qint64(1000));
    QCOMPARE(m_db->budget(2026, 12, expense, &found, &err), qint64(2000));
    QCOMPARE(m_db->budget(2026, 11, expense, &found, &err), qint64(500000));

    // 负预算拒绝
    QVERIFY(!m_db->setBudget(2026, 11, expense, -1, &err));
    // 非法月份拒绝
    QVERIFY(!m_db->setBudget(2026, 13, expense, 100, &err));

    // 删除
    QVERIFY(m_db->removeBudget(2026, 11, income, &err));
    QVERIFY(!m_db->budget(2026, 11, income, &found, &err));
    QVERIFY(!found);
}

void DatabaseTest::monthSeriesTrend()
{
    QString err;
    // 使用独立账本，避免与其它用例共享 m_db 的遗留数据干扰汇总断言
    zl::Database db(m_dir.filePath(QStringLiteral("series.sqlite")));
    QVERIFY(db.open(&err));

    const qint64 oct = QDateTime(QDate(2026, 10, 15), QTime(12, 0)).toMSecsSinceEpoch();
    const qint64 nov = QDateTime(QDate(2026, 11, 15), QTime(12, 0)).toMSecsSinceEpoch();
    const qint64 dec = QDateTime(QDate(2026, 12, 15), QTime(12, 0)).toMSecsSinceEpoch();

    zl::Transaction a = makeTx(zl::TxType::Expense, 1000, QStringLiteral("餐饮"), oct);
    zl::Transaction b = makeTx(zl::TxType::Income, 500000, QStringLiteral("工资"), nov);
    zl::Transaction c = makeTx(zl::TxType::Expense, 2000, QStringLiteral("交通"), dec);
    QVERIFY(db.insert(&a, &err));
    QVERIFY(db.insert(&b, &err));
    QVERIFY(db.insert(&c, &err));

    // 近 3 月（10/11/12），升序
    const QVector<zl::MonthPoint> series = db.monthSeries(2026, 12, 3, &err);
    QCOMPARE(series.size(), 3);
    QCOMPARE(series.at(0).year, 2026);
    QCOMPARE(series.at(0).month, 10);
    QCOMPARE(series.at(0).expenseCents, qint64(1000));
    QCOMPARE(series.at(0).incomeCents, qint64(0));
    QCOMPARE(series.at(1).month, 11);
    QCOMPARE(series.at(1).incomeCents, qint64(500000));
    QCOMPARE(series.at(2).month, 12);
    QCOMPARE(series.at(2).expenseCents, qint64(2000));

    // count<=0 返回空
    QVERIFY(db.monthSeries(2026, 12, 0, &err).isEmpty());
}

void DatabaseTest::dailySummaryAndByDate()
{
    QString err;
    zl::Database db(m_dir.filePath(QStringLiteral("daily.sqlite")));
    QVERIFY(db.open(&err));

    const qint64 d1 = QDateTime(QDate(2026, 11, 3), QTime(9, 0)).toMSecsSinceEpoch();
    const qint64 d2 = QDateTime(QDate(2026, 11, 3), QTime(20, 30)).toMSecsSinceEpoch();
    const qint64 d3 = QDateTime(QDate(2026, 11, 15), QTime(12, 0)).toMSecsSinceEpoch();
    zl::Transaction a = makeTx(zl::TxType::Expense, 1000, QStringLiteral("餐饮"), d1);
    zl::Transaction b = makeTx(zl::TxType::Expense, 2500, QStringLiteral("交通"), d2);
    zl::Transaction c = makeTx(zl::TxType::Income, 500000, QStringLiteral("工资"), d3);
    QVERIFY(db.insert(&a, &err));
    QVERIFY(db.insert(&b, &err));
    QVERIFY(db.insert(&c, &err));

    // 每日汇总：11/3 支出 35.00 共 2 笔；11/15 收入 5000.00 共 1 笔
    const QVector<zl::DaySummary> summary = db.monthDailySummary(2026, 11, &err);
    QCOMPARE(summary.size(), 2);
    QCOMPARE(summary.at(0).day, 3);
    QCOMPARE(summary.at(0).expenseCents, qint64(3500));
    QCOMPARE(summary.at(0).incomeCents, qint64(0));
    QCOMPARE(summary.at(0).count, 2);
    QCOMPARE(summary.at(1).day, 15);
    QCOMPARE(summary.at(1).incomeCents, qint64(500000));

    // 按日查询：11/3 两笔，按时间倒序（20:30 在前）
    const QVector<zl::Transaction> day3 = db.queryByDate(2026, 11, 3, &err);
    QCOMPARE(day3.size(), 2);
    QCOMPARE(day3.at(0).amountCents, qint64(2500));
    QCOMPARE(day3.at(1).amountCents, qint64(1000));
    // 空日与无效日
    QVERIFY(db.queryByDate(2026, 11, 4, &err).isEmpty());
    QVERIFY(db.queryByDate(2026, 11, 31, &err).isEmpty());
    err.clear();
    QVERIFY(db.queryByDate(2026, 11, 32, &err).isEmpty());
    QVERIFY(!err.isEmpty());
}

QTEST_MAIN(DatabaseTest)
#include "tst_database.moc"
