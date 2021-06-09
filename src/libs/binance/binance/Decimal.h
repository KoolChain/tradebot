#pragma once


#include <boost/multiprecision/cpp_dec_float.hpp>


namespace ad {

constexpr std::size_t DECIMALS = 8;

using Decimal = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<DECIMALS>>;


template <class T_floating>
Decimal fromFP(T_floating aFloating)
{
    std::ostringstream oss;
    oss.precision(DECIMALS);
    oss << std::fixed << aFloating;
    return Decimal{oss.str()};
}

/// \brief Returns a string representation with the exact amount of fixed decimals.
///
/// This method should be used to communicate Decimals overs the rest API.
inline std::string to_str(Decimal aDecimal)
{
    // IMPORTANT: explicit DECIMAL is important.
    // If a decimal is constructed from a string of <= precision, this is probably useless.
    // Yet, when constructed from a double, the approximation after the 8th appear in the string
    // when the "digits" argument is not provided.
    return aDecimal.str(DECIMALS, std::ios_base::fixed);
}


/// \brief This implements equality comparison on all the fixed decimals (but not more).
///
/// Allows to implement sanity checks without being bothered by inexact representations on
/// further decimals.
inline bool isEqual(Decimal aLhs, Decimal aRhs)
{
    return Decimal{to_str(aLhs)} == Decimal{to_str(aRhs)};
}


} // namespace ad
