#pragma once


#include <trademath/Decimal.h>
#include <trademath/FilterUtilities.h>


namespace ad {
namespace tradebot {


struct SymbolFilters
{
    struct ValueDomain
    {
        Decimal minimum{0};
        Decimal maximum{std::numeric_limits<Decimal>::max()};
        Decimal tickSize{trade::gDefaultTickSize};
    };

    ValueDomain price;
    ValueDomain amount;

    Decimal minimumNotional{0};
};


/// \return `true` if the filters are satisfied, `false` otherwise.
inline bool testAmount(const SymbolFilters & aFilters, Decimal aAmount, Decimal aRate)
{
    return (aAmount >= aFilters.amount.minimum)
        && (aAmount <= aFilters.amount.maximum)
        && (aAmount * aRate >= aFilters.minimumNotional)
            ;
}


} // namespace tradebot
} // namespace ad
