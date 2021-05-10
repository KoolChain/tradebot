#pragma once

#include <chrono>


namespace ad {


inline auto getTimestamp()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}


} // namespace ad
