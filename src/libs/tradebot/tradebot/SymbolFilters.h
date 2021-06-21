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
};


} // namespace tradebot
} // namespace ad
