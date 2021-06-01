#pragma once

#include <chrono>


namespace ad {


using MillisecondsSinceEpoch = long long;


inline MillisecondsSinceEpoch getTimestamp()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}


} // namespace ad
