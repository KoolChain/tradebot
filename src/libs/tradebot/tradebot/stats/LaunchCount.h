#pragma once


#include <binance/Time.h>


namespace ad {
namespace tradebot {
namespace stats {


struct Launch
{
    MillisecondsSinceEpoch time{getTimestamp()};

    long id{-1}; // auto-increment by ORM
};


} // namespace stats
} // namespace tradebot
} // namespace ad
