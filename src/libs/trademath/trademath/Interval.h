#pragma once


#include "Decimal.h"


namespace ad {

struct Interval
{
    Interval(Decimal aFront, Decimal aBack);

    Decimal front;
    Decimal back;
};


} // namespace ad
