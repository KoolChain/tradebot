#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>

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


template <class T_duration>
std::string timeToString(const std::chrono::time_point<std::chrono::system_clock,
                                                       T_duration> aTimePoint,
                         const std::string & aFormat = "%F %T")
{
    // The time point must be in the default system_clock duration for to_time_t()
    auto defTimePoint =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(aTimePoint);
    auto time = std::chrono::system_clock::to_time_t(defTimePoint);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), aFormat.c_str());
    return oss.str();
}


/// \brief Approximate conversion between different std::chrono clocks.
///
/// \note see: https://stackoverflow.com/a/35293183/1027706
template
<
  typename DstTimePointT,
  typename SrcTimePointT,
  typename DstClockT = typename DstTimePointT::clock,
  typename SrcClockT = typename SrcTimePointT::clock
>
DstTimePointT
clock_cast(const SrcTimePointT tp)
{
  const auto src_now = SrcClockT::now();
  const auto dst_now = DstClockT::now();
  return dst_now + (tp - src_now);
}


} // namespace ad
