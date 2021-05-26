#pragma once


#include "../Orders.h"

#include <cpr/cpr.h>


namespace ad {
namespace binance {
namespace detail {


inline cpr::Parameters initParameters(const MarketOrder & aOrder)
{
    return {
            {"symbol", aOrder.symbol},
            {"side", to_string(aOrder.side)},
            {"quantity", std::to_string(aOrder.quantity)},
            {"newClientOrderId", static_cast<const std::string &>(aOrder.clientId)},
            {"type", to_string(aOrder.type)}
    };
}


inline cpr::Parameters initParameters(const LimitOrder & aOrder)
{
    return {
            {"symbol", aOrder.symbol},
            {"side", to_string(aOrder.side)},
            {"quantity", std::to_string(aOrder.quantity)},
            {"newClientOrderId", static_cast<const std::string &>(aOrder.clientId)},
            {"price", std::to_string(aOrder.price)},
            {"timeInForce", to_string(aOrder.timeInForce)},
            {"type", to_string(aOrder.type)}
    };
}


} // namespace detail
} // namespace binance
} // namespace ad
