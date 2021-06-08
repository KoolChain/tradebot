#pragma once


#include <boost/multiprecision/cpp_dec_float.hpp>


namespace ad {


using Decimal = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<8>>;


inline std::string to_str(Decimal aDecimal)
{
    return aDecimal.str();
}

} // namespace ad
