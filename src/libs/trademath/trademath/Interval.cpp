#include "Interval.h"

#include "DecimalLog.h"

#include <spdlog/spdlog.h>

#include <exception>


namespace ad {


Interval::Interval(Decimal aFront, Decimal aBack) :
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
