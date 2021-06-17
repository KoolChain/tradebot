#pragma once

#include <trademath/Decimal.h>

#include <string>


namespace ad {
namespace tradebot {


using Coin = std::string;


enum class Side
{
    Buy,
    Sell,
};


inline Side reverse(Side aSide)
{
    return (aSide == Side::Sell ? Side::Buy : Side::Sell);
}


using FragmentId = long;

struct Fragment
{
    /// \brief Returns `true` if the fragment has no parent order, `false` otherwise.
    bool isInitial() const;

    std::string getIdentity() const;

    Coin base;
    Coin quote;
    Decimal amount;
    Decimal targetRate;
    Side side;

    Decimal takenHome{"0"};
    long spawningOrder{-1};
    long composedOrder{-1};
    FragmentId id{-1}; // auto-increment by ORM
};


/// \important Compares every data member except for the `id`!
bool operator==(const Fragment & aLhs, const Fragment & aRhs);

bool operator!=(const Fragment & aLhs, const Fragment & aRhs);



std::ostream & operator<<(std::ostream & aOut, const Fragment & aRhs);

inline std::ostream & operator<<(std::ostream & aOut, const Side & aSide)
{
    return aOut <<
        [](const Side & aSide) -> std::string
        {
            switch(aSide)
            {
                case Side::Sell:
                    return "Sell";
                case Side::Buy:
                    return "Buy";
            }
        }
        (aSide);
}


inline Side readSide(const std::string & aSide)
{
    if (aSide == "Sell" || aSide == "sell")
    {
        return Side::Sell;
    }
    else if (aSide == "Buy" || aSide == "buy")
    {
        return Side::Buy;
    }
    else
    {
        throw std::invalid_argument{"String '" + aSide + "' does not match to a Side value."};
    }
}


} // namespace tradebot
} // namespace ad
