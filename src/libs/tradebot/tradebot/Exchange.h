#pragma once


#include "Order.h"

#include <binance/Api.h>


namespace ad {
namespace tradebot {


struct Exchange
{
    std::string getOrderStatus(Order aOrder, const std::string & aTraderName);

    std::vector<binance::ClientId> cancelAllOpenOrders(const Pair & aPair);

    binance::Api restApi;
};


} // namespace tradebot
} // namespace ad
