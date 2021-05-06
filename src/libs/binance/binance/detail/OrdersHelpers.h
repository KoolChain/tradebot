#pragma once


#include "../Orders.h"

#include <cpr/cpr.h>


namespace ad {
namespace binance {
namespace detail {


cpr::Parameters initParameters(const MarketOrder & aOrder)
{
    return {
            {"symbol", aOrder.symbol},
            {"side", to_string(aOrder.side)},
            {"type", to_string(aOrder.type)},
            {"quantity", std::to_string(aOrder.quantity)}};

 //   aParameters.Add("symbol", aOrder.symbol);
 //   aParameters.Add("side", to_string(aOrder.side));
 //   aParameters.Add("type", to_string(aOrder.type));
 //   aParameters.Add("quantity", std::to_string(aOrder.quantity));
}


} // namespace detail
} // namespace binance
} // namespace ad
