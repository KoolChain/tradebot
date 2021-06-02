#pragma once

#include "Decimal.h"

#include <nlohmann/json.hpp>


namespace ad {

using Json = nlohmann::json;


/// \brief Helper to converts a Json string to a double.
///
/// It essentially relies on the fact it is not overload to cast the Json string to std::string,
/// removing the ambiguity of using std::stod directly.
inline Decimal jstod(const std::string & aString)
{
    return std::stod(aString);
};

} // namespace ad
