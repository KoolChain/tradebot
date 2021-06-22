#pragma once


#include <trademath/Decimal.h>


namespace ad {
namespace tradebot {


struct SymbolFilters
{
    struct ValueDomain
    {
        Decimal minimum{0};
        Decimal maximum{std::numeric_limits<Decimal>::max()};
        Decimal tickSize{Decimal{1} / pow(10, EXCHANGE_DECIMALS)};
    };

    ValueDomain price;
    ValueDomain amount;

    Decimal minimumNotional;
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
