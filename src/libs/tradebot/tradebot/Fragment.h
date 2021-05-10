#pragma once

#include <string>


namespace ad {
namespace tradebot {


using MillisecondsSinceEpoch = long long;

using Decimal = double;

enum class Direction
{
    Buy,
    Sell,
};


struct Fragment
{
    enum class Status
    {
        Inactive,
        Active,
        Fullfilled,
    };

    std::string base;
    std::string quote;
    Decimal amount;
    Decimal targetRate;
    Direction direction;

    long originOrder{-1};
    Status status{Status::Inactive};
    long id{-1}; // auto-increment by ORM
};


/// \important Compares every data member except for the `id`!
bool operator==(const Fragment & aLhs, const Fragment & aRhs);

bool operator!=(const Fragment & aLhs, const Fragment & aRhs);



std::ostream & operator<<(std::ostream & aOut, const Fragment & aRhs);


} // namespace tradebot
} // namespace ad
