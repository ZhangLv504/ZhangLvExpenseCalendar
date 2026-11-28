#include "Backup.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLatin1String>

#include "Money.h"

namespace zl {
namespace {

QString err(const QString &msg, QString *out)
{
    if (out)
        *out = msg;
    return msg;
}

// 从 JSON 字符串读出 int64；JSON 整数经 double 会丢精度，故金额/时间戳以字符串存储。
bool readInt64(const QJsonObject &obj, const char *key, std::int64_t *out)
{
    const QJsonValue v = obj.value(QLatin1String(key));
    if (!v.isString())
        return false;
    bool ok = false;
    const std::int64_t value = v.toString().toLongLong(&ok);
    if (!ok)
        return false;
    *out = value;
    return true;
}

bool readInt(const QJsonObject &obj, const char *key, int *out)
{
    const QJsonValue v = obj.value(QLatin1String(key));
    if (!v.isDouble() || v.toInt() != v.toDouble())
        return false;
    *out = v.toInt();
    return true;
}

} // namespace

QByteArray Backup::build(const BackupData &data)
{
    QJsonObject root;
    root.insert(QLatin1String("format"), QLatin1String("zhanglv-expense-backup"));
    root.insert(QLatin1String("schema"), kCurrentSchemaVersion);

    QJsonArray txArray;
    for (const Transaction &tx : data.transactions) {
        QJsonObject o;
        o.insert(QLatin1String("id"), QString::number(tx.id));
        o.insert(QLatin1String("type"), static_cast<int>(tx.type));
        o.insert(QLatin1String("amountCents"), QString::number(tx.amountCents));
        o.insert(QLatin1String("category"), tx.category);
        o.insert(QLatin1String("note"), tx.note);
        o.insert(QLatin1String("createdMs"), QString::number(tx.createdMs));
        txArray.append(o);
    }
    root.insert(QLatin1String("transactions"), txArray);

    QJsonArray catArray;
    for (const CategoryRecord &c : data.categories) {
        QJsonObject o;
        o.insert(QLatin1String("type"), c.type);
        o.insert(QLatin1String("name"), c.name);
        o.insert(QLatin1String("builtin"), c.builtin);
        catArray.append(o);
    }
    root.insert(QLatin1String("categories"), catArray);

    QJsonArray budgetArray;
    for (const BudgetRecord &b : data.budgets) {
        QJsonObject o;
        o.insert(QLatin1String("year"), b.year);
        o.insert(QLatin1String("month"), b.month);
        o.insert(QLatin1String("type"), b.type);
        o.insert(QLatin1String("amountCents"), QString::number(b.amountCents));
        budgetArray.append(o);
    }
    root.insert(QLatin1String("budgets"), budgetArray);

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Compact);
}

bool Backup::parse(const QByteArray &json, BackupData *out, QString *error)
{
    if (!out) {
        err(QStringLiteral("内部错误：备份输出对象为空"), error);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        err(QStringLiteral("备份文件不是有效的 JSON"), error);
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QLatin1String("format")).toString()
        != QLatin1String("zhanglv-expense-backup")) {
        err(QStringLiteral("备份文件格式不匹配"), error);
        return false;
    }
    int schema = 0;
    if (!readInt(root, "schema", &schema) || schema != kCurrentSchemaVersion) {
        err(QStringLiteral("备份文件版本不受支持"), error);
        return false;
    }

    BackupData parsed;
    const QJsonValue txVal = root.value(QLatin1String("transactions"));
    const QJsonValue catVal = root.value(QLatin1String("categories"));
    const QJsonValue budgetVal = root.value(QLatin1String("budgets"));
    if (!txVal.isArray() || !catVal.isArray() || !budgetVal.isArray()) {
        err(QStringLiteral("备份文件缺少必要的数据段"), error);
        return false;
    }

    const QJsonArray txArray = txVal.toArray();
    for (const QJsonValue &v : txArray) {
        if (!v.isObject()) {
            err(QStringLiteral("账目记录格式错误"), error);
            return false;
        }
        const QJsonObject o = v.toObject();
        Transaction tx;
        int type = 0;
        if (!readInt64(o, "id", &tx.id) || tx.id <= 0
            || !readInt(o, "type", &type)) {
            err(QStringLiteral("账目记录字段不合法"), error);
            return false;
        }
        if (type != 0 && type != 1) {
            err(QStringLiteral("账目类型不合法"), error);
            return false;
        }
        tx.type = (type == 1) ? TxType::Income : TxType::Expense;
        if (!readInt64(o, "amountCents", &tx.amountCents) || tx.amountCents <= 0) {
            err(QStringLiteral("账目金额不合法"), error);
            return false;
        }
        if (!readInt64(o, "createdMs", &tx.createdMs) || tx.createdMs <= 0) {
            err(QStringLiteral("账目日期不合法"), error);
            return false;
        }
        tx.category = o.value(QLatin1String("category")).toString().trimmed();
        tx.note = o.value(QLatin1String("note")).toString().trimmed();
        if (tx.category.isEmpty()) {
            err(QStringLiteral("账目分类不能为空"), error);
            return false;
        }
        parsed.transactions.append(tx);
    }

    const QJsonArray catArray = catVal.toArray();
    for (const QJsonValue &v : catArray) {
        if (!v.isObject()) {
            err(QStringLiteral("分类记录格式错误"), error);
            return false;
        }
        const QJsonObject o = v.toObject();
        CategoryRecord c;
        int type = 0;
        if (!readInt(o, "type", &type) || (type != 0 && type != 1)) {
            err(QStringLiteral("分类类型不合法"), error);
            return false;
        }
        c.type = type;
        c.name = o.value(QLatin1String("name")).toString().trimmed();
        if (c.name.isEmpty()) {
            err(QStringLiteral("分类名称不能为空"), error);
            return false;
        }
        c.builtin = o.value(QLatin1String("builtin")).toBool(false);
        parsed.categories.append(c);
    }

    const QJsonArray budgetArray = budgetVal.toArray();
    for (const QJsonValue &v : budgetArray) {
        if (!v.isObject()) {
            err(QStringLiteral("预算记录格式错误"), error);
            return false;
        }
        const QJsonObject o = v.toObject();
        BudgetRecord b;
        if (!readInt(o, "year", &b.year) || b.year < 2000 || b.year > 9999
            || !readInt(o, "month", &b.month) || b.month < 1 || b.month > 12
            || !readInt(o, "type", &b.type) || (b.type != 0 && b.type != 1)
            || !readInt64(o, "amountCents", &b.amountCents) || b.amountCents < 0) {
            err(QStringLiteral("预算记录字段不合法"), error);
            return false;
        }
        parsed.budgets.append(b);
    }

    *out = parsed;
    return true;
}

} // namespace zl
