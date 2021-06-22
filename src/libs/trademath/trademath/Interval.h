#pragma once


#include "Decimal.h"

#include <exception>


namespace ad {

struct Interval
{
    Interval(Decimal aFront, Decimal aBack);

    Decimal front;
    Decimal back;
};


inline Interval::Interval(Decimal aFront, Decimal aBack) :
    front{aFront},
    back{aBack}
{
    if (front > back)
    {
        spdlog::critical("Invalid interval [{}, {}].", front, back);
        throw std::invalid_argument("Invalid interval.");
    }
}


} // namespace ad
