#pragma once

#include <cstdint>
#include <limits>

namespace charworkspace {

struct Quote {
    int amount, quantity;
};

enum class QuoteResult { Ready, Changed, InsufficientFunds, WalletFull };

inline int StackPrice(int unitPrice, int quantity) {
    const auto total = int64_t(unitPrice) * quantity;
    return unitPrice > 0 && quantity > 0 && total <= (std::numeric_limits<int>::max)()
        ? static_cast<int>(total) : 0;
}

inline QuoteResult ValidateQuote(Quote expected, Quote actual, int funds, bool purchase) {
    if (actual.amount <= 0 || actual.quantity <= 0 || expected.amount != actual.amount
        || expected.quantity != actual.quantity) return QuoteResult::Changed;
    if (purchase && actual.amount > funds) return QuoteResult::InsufficientFunds;
    if (!purchase && int64_t(funds) + actual.amount > (std::numeric_limits<int>::max)())
        return QuoteResult::WalletFull;
    return QuoteResult::Ready;
}

}
