#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "Transaction.h"

namespace zl {

// 账目列表模型：供 QML ListView 绑定。角色名保持英文稳定标识，展示格式化在 data() 完成。
class TransactionModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TypeRole,
        AmountTextRole,
        CategoryRole,
        NoteRole,
        TimeTextRole
    };
    Q_ENUM(Roles)

    explicit TransactionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 用一批账目替换当前内容
    void setTransactions(const QVector<Transaction> &items);

    // 取出第 row 行账目（供编辑/删除）
    Transaction at(int row) const;

    // 当前行数（QML 空态判断用；QAbstractListModel 本身不暴露 count）
    int count() const;

signals:
    void countChanged();

private:
    QVector<Transaction> m_items;
};

} // namespace zl
