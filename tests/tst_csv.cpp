#include <QtTest>
#include <QDateTime>

#include "CsvExporter.h"
#include "Money.h"

class CsvTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyList();
    void basicRows();
    void commaAndQuoteEscape();
    void formulaGuard();
    void noFormulaGuardForAmount();
    void utf8Bom();
    void largeAmount();
};

void CsvTest::emptyList()
{
    const QByteArray csv = zl::CsvExporter::build({});
    QVERIFY(csv.startsWith("\xEF\xBB\xBF"));
    const QString text = QString::fromUtf8(csv.constData() + 3);
    QCOMPARE(text, QStringLiteral("日期,类型,分类,金额(元),备注\r\n"));
}

void CsvTest::basicRows()
{
    zl::Transaction tx;
    tx.type = zl::TxType::Expense;
    tx.amountCents = 1234;
    tx.category = QStringLiteral("餐饮");
    tx.note = QStringLiteral("lunch");
    tx.createdMs = QDateTime(QDate(2026, 11, 25), QTime(14, 30, 0)).toMSecsSinceEpoch();

    zl::Transaction income;
    income.type = zl::TxType::Income;
    income.amountCents = 500000;
    income.category = QStringLiteral("工资");
    income.note = QStringLiteral("salary");
    income.createdMs = QDateTime(QDate(2026, 11, 26), QTime(9, 0, 0)).toMSecsSinceEpoch();

    const QString text = QString::fromUtf8(zl::CsvExporter::build({tx, income}).constData() + 3);
    const QStringList lines = text.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 3);
    QVERIFY(lines.at(1).contains(QStringLiteral("2026-11-25")));
    QVERIFY(!lines.at(1).contains(QStringLiteral("14:30"))); // 无时分
    QVERIFY(lines.at(1).contains(QStringLiteral("支出")));
    QVERIFY(lines.at(1).contains(QStringLiteral("餐饮")));
    QVERIFY(lines.at(1).contains(QStringLiteral("12.34")));
    QVERIFY(lines.at(1).contains(QStringLiteral("lunch")));
    QVERIFY(lines.at(2).contains(QStringLiteral("收入")));
    QVERIFY(lines.at(2).contains(QStringLiteral("5000.00")));
}

void CsvTest::commaAndQuoteEscape()
{
    // 分类含逗号，备注含引号和逗号
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("a,b"), true),
             QStringLiteral("\"a,b\""));
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("a\"b"), true),
             QStringLiteral("\"a\"\"b\""));
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("a,b\"c"), true),
             QStringLiteral("\"a,b\"\"c\""));
    // 换行也需包裹
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("a\nb"), true),
             QStringLiteral("\"a\nb\""));
    // 普通字段原样
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("plain"), true),
             QStringLiteral("plain"));
}

void CsvTest::formulaGuard()
{
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("=SUM(1+1)"), true),
             QStringLiteral("'=SUM(1+1)"));
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("+cmd"), true),
             QStringLiteral("'+cmd"));
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("@SUM"), true),
             QStringLiteral("'@SUM"));
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("-1"), true),
             QStringLiteral("'-1"));
    // 非首字符不防护
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("a=1"), true),
             QStringLiteral("a=1"));
    // 可关闭防护（金额列）
    QCOMPARE(zl::CsvExporter::escapeField(QStringLiteral("=x"), false),
             QStringLiteral("=x"));
}

void CsvTest::noFormulaGuardForAmount()
{
    zl::Transaction tx;
    tx.type = zl::TxType::Expense;
    tx.amountCents = 999;
    tx.category = QStringLiteral("=危险分类");
    tx.note = QStringLiteral("@公式备注");
    tx.createdMs = QDateTime(QDate(2026, 11, 25), QTime(10, 0, 0)).toMSecsSinceEpoch();

    const QString text = QString::fromUtf8(zl::CsvExporter::build({tx}).constData() + 3);
    const QString line = text.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts).at(1);
    const QStringList cells = line.split(QLatin1Char(','));
    QCOMPARE(cells.at(2), QStringLiteral("'=危险分类"));
    QCOMPARE(cells.at(3), QStringLiteral("9.99"));  // 金额不被误加防护引号
    QCOMPARE(cells.at(4), QStringLiteral("'@公式备注"));
}

void CsvTest::utf8Bom()
{
    zl::Transaction tx;
    tx.amountCents = 100;
    tx.category = QStringLiteral("中文");
    tx.note = QStringLiteral("测试");
    tx.createdMs = QDateTime(QDate(2026, 11, 25), QTime(10, 0, 0)).toMSecsSinceEpoch();

    const QByteArray csv = zl::CsvExporter::build({tx});
    QCOMPARE(static_cast<unsigned char>(csv.at(0)), 0xEF);
    QCOMPARE(static_cast<unsigned char>(csv.at(1)), 0xBB);
    QCOMPARE(static_cast<unsigned char>(csv.at(2)), 0xBF);
    QVERIFY(csv.contains(QStringLiteral("中文").toUtf8()));
}

void CsvTest::largeAmount()
{
    zl::Transaction tx;
    tx.amountCents = 999999999999; // 亿元级
    tx.category = QStringLiteral("理财");
    tx.createdMs = QDateTime(QDate(2026, 11, 25), QTime(10, 0, 0)).toMSecsSinceEpoch();
    const QString text = QString::fromUtf8(zl::CsvExporter::build({tx}).constData() + 3);
    QVERIFY(text.contains(zl::Money::format(tx.amountCents)));
    QVERIFY(text.contains(QStringLiteral("9999999999.99")));
}

QTEST_MAIN(CsvTest)
#include "tst_csv.moc"
