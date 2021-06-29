#pragma once

#include <chrono>

#include <ctime>


namespace ad {


using MillisecondsSinceEpoch = long long;


inline MillisecondsSinceEpoch getTimestamp()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}


/// \return Milliseconds since epoch of the timepoint being today at midnight.
inline MillisecondsSinceEpoch getMidnightTimestamp()
{
    using clock = std::chrono::system_clock;
    std::time_t now = clock::to_time_t(clock::now());
    std::tm * date = std::localtime(&now);
    date->tm_hour = 0;
    date->tm_min = 0;
    date->tm_sec = 0;

    auto durationSinceEpoch = clock::from_time_t(mktime(date)).time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(durationSinceEpoch).count();
}


} // namespace ad
