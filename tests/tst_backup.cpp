#include <QtTest>
#include <QDateTime>
#include <QTemporaryDir>

#include "Backup.h"
#include "Database.h"
#include "Money.h"

class BackupTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void largeAmountPrecision();
    void invalidJson();
    void wrongFormat();
    void unsupportedVersion();
    void invalidRecord();
    void databaseRoundTrip();
    void importRollbackOnDuplicateCategory();

private:
    static zl::Transaction makeTx(std::int64_t id, std::int64_t cents,
                                  const QString &cat, const QString &note,
                                  const QDate &date)
    {
        zl::Transaction tx;
        tx.id = id;
        tx.type = zl::TxType::Expense;
        tx.amountCents = cents;
        tx.category = cat;
        tx.note = note.isEmpty() ? QStringLiteral("") : note;
        tx.createdMs = QDateTime(date, QTime(0, 0, 0)).toMSecsSinceEpoch();
        return tx;
    }
};

void BackupTest::roundTrip()
{
    zl::BackupData data;
    data.transactions.append(makeTx(1, 1234, QStringLiteral("餐饮"),
                                    QStringLiteral("lunch"), QDate(2026, 11, 27)));
    data.categories.append({QStringLiteral("餐饮"), 0, true});
    data.categories.append({QStringLiteral("宠物"), 0, false});
    data.budgets.append({2026, 11, 0, 100000});

    const QByteArray json = zl::Backup::build(data);
    zl::BackupData parsed;
    QString error;
    QVERIFY2(zl::Backup::parse(json, &parsed, &error), qPrintable(error));
    QCOMPARE(parsed.transactions.size(), 1);
    QCOMPARE(parsed.transactions.at(0).id, qlonglong(1));
    QCOMPARE(parsed.transactions.at(0).amountCents, qlonglong(1234));
    QCOMPARE(parsed.transactions.at(0).category, QStringLiteral("餐饮"));
    QCOMPARE(parsed.transactions.at(0).note, QStringLiteral("lunch"));
    QCOMPARE(parsed.transactions.at(0).createdMs,
             data.transactions.at(0).createdMs);
    QCOMPARE(parsed.categories.size(), 2);
    QCOMPARE(parsed.categories.at(0).name, QStringLiteral("餐饮"));
    QVERIFY(parsed.categories.at(0).builtin);
    QVERIFY(!parsed.categories.at(1).builtin);
    QCOMPARE(parsed.budgets.size(), 1);
    QCOMPARE(parsed.budgets.at(0).amountCents, qlonglong(100000));
}

void BackupTest::largeAmountPrecision()
{
    zl::BackupData data;
    data.transactions.append(makeTx(1, 999999999999LL, QStringLiteral("理财"),
                                    QString(), QDate(2026, 11, 28)));
    const QByteArray json = zl::Backup::build(data);
    zl::BackupData parsed;
    QString error;
    QVERIFY(zl::Backup::parse(json, &parsed, &error));
    QCOMPARE(parsed.transactions.at(0).amountCents, qlonglong(999999999999LL));
    QCOMPARE(zl::Money::format(parsed.transactions.at(0).amountCents),
             QStringLiteral("9999999999.99"));
}

void BackupTest::invalidJson()
{
    zl::BackupData out;
    QString error;
    QVERIFY(!zl::Backup::parse(QByteArray("not json"), &out, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!zl::Backup::parse(QByteArray("[]"), &out, &error));
}

void BackupTest::wrongFormat()
{
    zl::BackupData out;
    QString error;
    const QByteArray json = "{\"format\":\"other\",\"schema\":1}";
    QVERIFY(!zl::Backup::parse(json, &out, &error));
    QVERIFY(error.contains(QStringLiteral("格式")));
}

void BackupTest::unsupportedVersion()
{
    zl::BackupData out;
    QString error;
    const QByteArray json =
        "{\"format\":\"zhanglv-expense-backup\",\"schema\":99,"
        "\"transactions\":[],\"categories\":[],\"budgets\":[]}";
    QVERIFY(!zl::Backup::parse(json, &out, &error));
    QVERIFY(error.contains(QStringLiteral("版本")));
}

void BackupTest::invalidRecord()
{
    const QString base = QStringLiteral(
        "{\"format\":\"zhanglv-expense-backup\",\"schema\":1,"
        "\"transactions\":[%1],\"categories\":[],\"budgets\":[]}");
    zl::BackupData out;
    QString error;
    // 金额为零非法
    QVERIFY(!zl::Backup::parse(
        base.arg(QStringLiteral("{\"id\":\"1\",\"type\":0,\"amountCents\":\"0\","
                                "\"category\":\"餐饮\",\"note\":\"\",\"createdMs\":\"1\"}"))
            .toUtf8(), &out, &error));
    // 分类为空非法
    QVERIFY(!zl::Backup::parse(
        base.arg(QStringLiteral("{\"id\":\"1\",\"type\":0,\"amountCents\":\"100\","
                                "\"category\":\"\",\"note\":\"\",\"createdMs\":\"1\"}"))
            .toUtf8(), &out, &error));
}

void BackupTest::databaseRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/ledger.sqlite");

    zl::Database source(path);
    QVERIFY(source.open());
    zl::Transaction tx = makeTx(0, 1234, QStringLiteral("餐饮"),
                                QStringLiteral("lunch"), QDate(2026, 11, 27));
    QVERIFY(source.insert(&tx));
    QVERIFY(source.addCategory(0, QStringLiteral("宠物")));
    QVERIFY(source.setBudget(2026, 11, 0, 100000));

    const zl::BackupData data = source.exportAll();
    QCOMPARE(data.transactions.size(), 1);
    QVERIFY(data.categories.size() > 1); // 内置分类 + 1 自定义
    QCOMPARE(data.budgets.size(), 1);

    // 用另一实例导入，验证数据完整
    const QString path2 = dir.path() + QStringLiteral("/ledger2.sqlite");
    zl::Database target(path2);
    QVERIFY(target.open());
    QVERIFY(target.importAll(data));
    QCOMPARE(target.count(), 1);

    const zl::BackupData again = target.exportAll();
    QCOMPARE(again.transactions.at(0).amountCents, qlonglong(1234));
    QCOMPARE(again.transactions.at(0).category, QStringLiteral("餐饮"));
    QCOMPARE(again.categories.size(), data.categories.size());
    QCOMPARE(again.budgets.at(0).amountCents, qlonglong(100000));

    // 原有库未被破坏
    QCOMPARE(source.count(), 1);
}

void BackupTest::importRollbackOnDuplicateCategory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    zl::Database db(dir.path() + QStringLiteral("/ledger.sqlite"));
    QVERIFY(db.open());

    zl::Transaction tx = makeTx(0, 500, QStringLiteral("餐饮"),
                                QString(), QDate(2026, 11, 20));
    QString insertError;
    QVERIFY2(db.insert(&tx, &insertError), qPrintable(insertError));

    zl::BackupData bad;
    bad.transactions.append(makeTx(99, 999, QStringLiteral("餐饮"),
                                   QString(), QDate(2026, 11, 28)));
    // 同名分类重复，触发 UNIQUE 约束，整体回滚
    bad.categories.append({QStringLiteral("餐饮"), 0, false});
    bad.categories.append({QStringLiteral("餐饮"), 0, false});

    QString error;
    QVERIFY(!db.importAll(bad, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(db.count(), 1); // 原有数据完整保留
}

QTEST_MAIN(BackupTest)
#include "tst_backup.moc"
