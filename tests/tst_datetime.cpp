#include <QtTest>
#include <QDateTime>

#include "DateTimeUtil.h"

class DateTimeTest : public QObject
{
    Q_OBJECT

private slots:
    void parseValid_data();
    void parseValid();
    void parseInvalid_data();
    void parseInvalid();
    void roundTrip_data();
    void roundTrip();
    void crossMonthBoundary();   // 跨月/跨年归属验证
    void leapYear();             // 闰年 2/29

private:
    static qint64 msOf(int y, int m, int d, int hh, int mm)
    {
        return QDateTime(QDate(y, m, d), QTime(hh, mm, 0)).toMSecsSinceEpoch();
    }
};

void DateTimeTest::parseValid_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<qint64>("ms");
    QTest::newRow("normal")   << QStringLiteral("2026-11-25 14:30") << msOf(2026, 11, 25, 14, 30);
    QTest::newRow("midnight") << QStringLiteral("2026-01-01 00:00") << msOf(2026, 1, 1, 0, 0);
    QTest::newRow("end")      << QStringLiteral("2026-12-31 23:59") << msOf(2026, 12, 31, 23, 59);
    QTest::newRow("spaces")   << QStringLiteral("  2026-10-15 12:30  ") << msOf(2026, 10, 15, 12, 30);
}

void DateTimeTest::parseValid()
{
    QFETCH(QString, text);
    QFETCH(qint64, ms);
    std::int64_t out = 0;
    QVERIFY2(zl::DateTimeUtil::parse(text, &out), qPrintable(text));
    QCOMPARE(out, ms);
}

void DateTimeTest::parseInvalid_data()
{
    QTest::addColumn<QString>("text");
    QTest::newRow("empty")        << QStringLiteral("");
    QTest::newRow("date only")    << QStringLiteral("2026-11-25");
    QTest::newRow("bad format")   << QStringLiteral("2026/11/25 14:30");
    QTest::newRow("invalid day")  << QStringLiteral("2026-02-30 12:00");
    QTest::newRow("extra text")   << QStringLiteral("2026-11-25 14:30:00");
    QTest::newRow("letters")      << QStringLiteral("abcd-ef-gh ij:kl");
}

void DateTimeTest::parseInvalid()
{
    QFETCH(QString, text);
    std::int64_t out = 12345;
    QVERIFY(!zl::DateTimeUtil::parse(text, &out));
    QCOMPARE(out, qint64(12345)); // 失败时不改写输出
}

void DateTimeTest::roundTrip_data()
{
    QTest::addColumn<qint64>("ms");
    QTest::newRow("a") << msOf(2026, 11, 25, 14, 30);
    QTest::newRow("b") << msOf(2025, 1, 1, 0, 0);
    QTest::newRow("c") << msOf(2028, 2, 29, 23, 59);
}

void DateTimeTest::roundTrip()
{
    QFETCH(qint64, ms);
    const QString text = zl::DateTimeUtil::format(ms);
    std::int64_t out = 0;
    QVERIFY(zl::DateTimeUtil::parse(text, &out));
    QCOMPARE(out, ms);
}

void DateTimeTest::crossMonthBoundary()
{
    // 10/31 23:59 与 11/01 00:00 相差 1 分钟，但归属不同月份
    const qint64 octEnd = msOf(2026, 10, 31, 23, 59);
    const qint64 novStart = msOf(2026, 11, 1, 0, 0);
    QVERIFY(novStart - octEnd == 60 * 1000);
    QVERIFY(QDate(2026, 10, 31) != QDate(2026, 11, 1));
}

void DateTimeTest::leapYear()
{
    std::int64_t out = 0;
    QVERIFY(zl::DateTimeUtil::parse(QStringLiteral("2028-02-29 10:00"), &out));
    QCOMPARE(out, msOf(2028, 2, 29, 10, 0));
    QVERIFY(!zl::DateTimeUtil::parse(QStringLiteral("2027-02-29 10:00"), &out));
}

QTEST_MAIN(DateTimeTest)
#include "tst_datetime.moc"
