#include "Transaction.h"

namespace zl {

bool isValidTransaction(const Transaction &tx)
{
    return tx.amountCents >= 0
        && tx.createdMs >= 0
        && !tx.category.trimmed().isEmpty();
}

} // namespace zl
