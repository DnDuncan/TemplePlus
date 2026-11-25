#include "../../TemplePlus/ui/charworkspace_quote.h"

#include <cassert>
#include <climits>
#include <iostream>

using namespace charworkspace;

int main() {
    assert(StackPrice(125, 4) == 500);
    assert(StackPrice(0, 4) == 0);
    assert(StackPrice(-2, -3) == 0);
    assert(StackPrice(INT_MAX, 2) == 0);
    assert(ValidateQuote({500,4},{500,4},500,true) == QuoteResult::Ready);
    assert(ValidateQuote({500,4},{500,4},499,true) == QuoteResult::InsufficientFunds);
    assert(ValidateQuote({500,4},{500,5},1000,true) == QuoteResult::Changed);
    assert(ValidateQuote({500,4},{525,4},1000,false) == QuoteResult::Changed);
    assert(ValidateQuote({0,1},{0,1},1000,true) == QuoteResult::Changed);
    assert(ValidateQuote({500,4},{500,4},INT_MAX-499,false) == QuoteResult::WalletFull);
    assert(ValidateQuote({500,4},{500,4},INT_MAX-500,false) == QuoteResult::Ready);
    std::cout << "Character workspace quote checks passed\n";
}
