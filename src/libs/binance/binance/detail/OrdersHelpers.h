#pragma once


#include "../Orders.h"

#include <cpr/cpr.h>

#include <spdlog/spdlog.h>


namespace ad {
namespace binance {
namespace detail {


inline const char * getQuantityKey(const OrderBase & aOrder)
{
    switch(aOrder.unit)
    {
        case QuantityUnit::Base:
            return "quantity";
        case QuantityUnit::Quote:
            return "quoteOrderQty";
        default:
            spdlog::warn("Order quantity unit unknown '{}', assuming 'base'.",
                         static_cast<int>(aOrder.unit));
            return "quantity";
    }
}


inline cpr::Parameters initParameters(const MarketOrder & aOrder)
{
    return {
            {"symbol", aOrder.symbol},
            {"side", to_string(aOrder.side)},
            {getQuantityKey(aOrder), to_str(aOrder.quantity)},
            {"newClientOrderId", static_cast<const std::string &>(aOrder.clientId)},
            {"type", to_string(aOrder.type)}
    };
}


inline cpr::Parameters initParameters(const LimitOrder & aOrder)
{
    return {
            {"symbol", aOrder.symbol},
            {"side", to_string(aOrder.side)},
            {getQuantityKey(aOrder), to_str(aOrder.quantity)},
            {"newClientOrderId", static_cast<const std::string &>(aOrder.clientId)},
            {"price", to_str(aOrder.price)},
            {"timeInForce", to_string(aOrder.timeInForce)},
            {"type", to_string(aOrder.type)}
    };
}


} // namespace detail
} // namespace binance
} // namespace ad
