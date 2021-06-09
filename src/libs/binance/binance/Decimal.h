#pragma once


#include <boost/multiprecision/cpp_dec_float.hpp>


namespace ad {


using Decimal = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<8>>;


inline std::string to_str(Decimal aDecimal)
{
    // IMPORTANT: explicit 8 is important.
    // If a decimal is constructed from a string of <= precision, this is probably useless.
    // Yet, when constructed from a double, the approximation after the 8th appear in the string
    // when the "digits" argument is not provided.
    return aDecimal.str(8);
}

} // namespace ad
