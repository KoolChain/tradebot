#pragma once


#include <binance/Decimal.h>
#include <binance/Json.h>
#include <binance/Time.h>


#include <string>


namespace ad {
namespace tradebot {


class Order;


struct Fulfillment
{
    binance::Decimal amountBase{0};
    binance::Decimal amountQuote{0};
    binance::Decimal fee{0};
    std::string feeAsset;
    MillisecondsSinceEpoch latestTrade{0};
    int tradeCount{0};

    binance::Decimal price() const
    {
        return amountQuote / amountBase;
    }

    Fulfillment & accumulate(const Fulfillment & aRhs, const Order & aContext);

    static Fulfillment fromTradeJson(const Json & aTrade);
};


inline Fulfillment Fulfillment::fromTradeJson(const Json & aTrade)
{
    return {
        jstod(aTrade.at("qty")),
        jstod(aTrade.at("quoteQty")),
        jstod(aTrade.at("commission")),
        aTrade.at("commissionAsset"),
        aTrade.value("time", 0),
        1, // 1 trade
    };
}


} // namespace tradebot
} // namespace ad
