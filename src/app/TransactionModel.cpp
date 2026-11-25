#include "TransactionModel.h"

#include <QDateTime>
#include "Money.h"

namespace zl {

TransactionModel::TransactionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TransactionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant TransactionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return QVariant();

    const Transaction &tx = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return QVariant::fromValue(tx.id);
    case TypeRole:
        return static_cast<int>(tx.type);
    case AmountTextRole:
        return Money::format(tx.amountCents);
    case CategoryRole:
        return tx.category;
    case NoteRole:
        return tx.note;
    case TimeTextRole:
        return QDateTime::fromMSecsSinceEpoch(tx.createdMs)
            .toString(QStringLiteral("MM-dd hh:mm"));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TransactionModel::roleNames() const
{
    return {
        { IdRole,        "txId" },
        { TypeRole,      "txType" },
        { AmountTextRole,"amountText" },
        { CategoryRole,  "category" },
        { NoteRole,      "note" },
        { TimeTextRole,  "timeText" }
    };
}

void TransactionModel::setTransactions(const QVector<Transaction> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
}

Transaction TransactionModel::at(int row) const
{
    if (row < 0 || row >= m_items.size())
        return Transaction();
    return m_items.at(row);
}

} // namespace zl
