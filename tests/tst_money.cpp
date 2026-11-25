#include <QtTest>
#include "Money.h"

class MoneyTest : public QObject
{
    Q_OBJECT

private slots:
    void parseValid_data();
    void parseValid();
    void parseInvalid_data();
    void parseInvalid();
    void format_data();
    void format();
    void roundTrip_data();
    void roundTrip();
    void parseOverflow();
};

void MoneyTest::parseValid_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<qint64>("cents");
    QTest::newRow("integer")      << QStringLiteral("5")       << qint64(500);
    QTest::newRow("one decimal")  << QStringLiteral("5.2")     << qint64(520);
    QTest::newRow("two decimals") << QStringLiteral("5.20")    << qint64(520);
    QTest::newRow("zero")         << QStringLiteral("0")       << qint64(0);
    QTest::newRow("zero cent")    << QStringLiteral("0.01")    << qint64(1);
    QTest::newRow("negative")     << QStringLiteral("-0.01")   << qint64(-1);
    QTest::newRow("big")          << QStringLiteral("12345678.90") << qint64(1234567890);
    QTest::newRow("spaces")       << QStringLiteral("  12.34 ") << qint64(1234);
    QTest::newRow("leading dot")  << QStringLiteral(".5")      << qint64(50);
}

void MoneyTest::parseValid()
{
    QFETCH(QString, input);
    QFETCH(qint64, cents);
    std::int64_t out = -1;
    QVERIFY2(zl::Money::parse(input, &out), qPrintable(input));
    QCOMPARE(out, cents);
}

void MoneyTest::parseInvalid_data()
{
    QTest::addColumn<QString>("input");
    QTest::newRow("empty")        << QStringLiteral("");
    QTest::newRow("letters")      << QStringLiteral("abc");
    QTest::newRow("third decimal")<< QStringLiteral("1.234");
    QTest::newRow("two dots")     << QStringLiteral("1.2.3");
    QTest::newRow("just minus")   << QStringLiteral("-");
    QTest::newRow("just dot")     << QStringLiteral(".");
    QTest::newRow("trailing junk")<< QStringLiteral("12.3x");
    QTest::newRow("comma")        << QStringLiteral("1,23");
}

void MoneyTest::parseInvalid()
{
    QFETCH(QString, input);
    std::int64_t out = 999;
    QVERIFY(!zl::Money::parse(input, &out));
}

void MoneyTest::parseOverflow()
{
    std::int64_t out = 0;
    // 超过 int64 分上限
    QVERIFY(!zl::Money::parse(QStringLiteral("99999999999999999999"), &out));
}

void MoneyTest::format_data()
{
    QTest::addColumn<qint64>("cents");
    QTest::addColumn<QString>("text");
    QTest::newRow("zero")      << qint64(0)    << QStringLiteral("0.00");
    QTest::newRow("one cent")  << qint64(1)    << QStringLiteral("0.01");
    QTest::newRow("ten cents") << qint64(10)   << QStringLiteral("0.10");
    QTest::newRow("yuan")      << qint64(100)  << QStringLiteral("1.00");
    QTest::newRow("mixed")     << qint64(1234) << QStringLiteral("12.34");
    QTest::newRow("negative")  << qint64(-5)   << QStringLiteral("-0.05");
    QTest::newRow("neg mixed") << qint64(-1234)<< QStringLiteral("-12.34");
}

void MoneyTest::format()
{
    QFETCH(qint64, cents);
    QFETCH(QString, text);
    QCOMPARE(zl::Money::format(cents), text);
}

void MoneyTest::roundTrip_data()
{
    QTest::addColumn<qint64>("cents");
    QTest::newRow("a") << qint64(1);
    QTest::newRow("b") << qint64(999);
    QTest::newRow("c") << qint64(100000);
    QTest::newRow("d") << qint64(-250);
}

void MoneyTest::roundTrip()
{
    QFETCH(qint64, cents);
    const QString text = zl::Money::format(cents);
    std::int64_t out = 0;
    QVERIFY(zl::Money::parse(text, &out));
    QCOMPARE(out, cents);
}

QTEST_MAIN(MoneyTest)
#include "tst_money.moc"
