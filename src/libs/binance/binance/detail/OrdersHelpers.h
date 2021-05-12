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
            {"quantity", std::to_string(aOrder.quantity)}
    };
}

cpr::Parameters initParameters(const LimitOrder & aOrder)
{
    return {
            {"symbol", aOrder.symbol},
            {"side", to_string(aOrder.side)},
            {"type", to_string(aOrder.type)},
            {"quantity", std::to_string(aOrder.quantity)},
            {"price", std::to_string(aOrder.price)},
            {"timeInForce", to_string(aOrder.timeInForce)}
    };
}


} // namespace detail
} // namespace binance
} // namespace ad
