#pragma once

#include <string>


namespace ad {
namespace tradebot {


using Coin = std::string;


using Decimal = double;


enum class Side
{
    Buy,
    Sell,
};


struct Fragment
{
    Coin base;
    Coin quote;
    Decimal amount;
    Decimal targetRate;
    Side side;

    long spawningOrder{-1};
    long composedOrder{-1};
    long id{-1}; // auto-increment by ORM
};


/// \important Compares every data member except for the `id`!
bool operator==(const Fragment & aLhs, const Fragment & aRhs);

bool operator!=(const Fragment & aLhs, const Fragment & aRhs);



std::ostream & operator<<(std::ostream & aOut, const Fragment & aRhs);


} // namespace tradebot
} // namespace ad
