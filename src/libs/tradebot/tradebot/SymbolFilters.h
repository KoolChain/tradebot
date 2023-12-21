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


inline std::ostream & operator<<(std::ostream & aOut, const SymbolFilters::ValueDomain & aValueDomain)
{
    return aOut << "min: " << aValueDomain.minimum
        << ", max: " << aValueDomain.maximum
        << ", tickSize: " << aValueDomain.tickSize
        ;
}

inline std::ostream & operator<<(std::ostream & aOut, const SymbolFilters & aFilters)
{
    return aOut << "Price  [" << aFilters.price << "]"
        << "\nAmount  [" << aFilters.amount << "]"
        << "\nMin notional:  " << aFilters.minimumNotional 
        ;
}


/// \return `true` if the filters are satisfied, `false` otherwise.
inline bool testAmount(const SymbolFilters & aFilters, Decimal aAmount, Decimal aRate)
{
    return (aAmount >= aFilters.amount.minimum)
        && (aAmount <= aFilters.amount.maximum)
        && (aAmount * aRate >= aFilters.minimumNotional)
            ;
}


/// \return `true` if the filters are satisfied, `false` otherwise.
inline bool testPrice(const SymbolFilters & aFilters, Decimal aPrice)
{
    return (aPrice >= aFilters.price.minimum)
        && (aPrice <= aFilters.price.maximum)
        // There is no modulus operator on our Decimal type, so test remainder is zero.
        && (trade::computeTickFilter(aPrice, aFilters.price.tickSize).second == 0)
            ;
}

} // namespace tradebot
} // namespace ad
