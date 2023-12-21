#pragma once


#include <trademath/Decimal.h>
#include <binance/Json.h>
#include <binance/Time.h>


#include <string>


namespace ad {
namespace tradebot {


class Order;


struct Fulfillment
{
    Decimal amountBase{0};
    Decimal amountQuote{0};
    Decimal fee{0};
    std::string feeAsset;
    MillisecondsSinceEpoch latestTrade{0};
    int tradeCount{0};

    Decimal price() const
    {
        return amountQuote / amountBase;
    }

    Fulfillment & accumulate(const Fulfillment & aRhs, const Order & aContext);

    /// \brief Intended for the Json objects returned by account trade list
    static Fulfillment fromTradeJson(const Json & aTrade);
    /// \brief Intended for the Json objects returned in place order response's "fills" array
    static Fulfillment fromFillJson(const Json & aTrade);
    /// \brief Intended for the Json execution reports published by the spot user data stream
    static Fulfillment fromStreamJson(const Json & aTrade);
};


inline Fulfillment Fulfillment::fromTradeJson(const Json & aTrade)
{
    return {
        jstod(aTrade.at("qty")),
        jstod(aTrade.at("quoteQty")),
        jstod(aTrade.at("commission")),
        aTrade.at("commissionAsset"),
        aTrade.at("time"),
        1, // 1 trade
    };
}


inline Fulfillment Fulfillment::fromFillJson(const Json & aTrade)
{
    return {
        .amountBase = jstod(aTrade.at("qty")),
        .amountQuote = 0, // The individual fills do not contain the quote qty.
            // The cumulative quote qty, available on the overall order response, has to be set by the calling context.
            // (e.g. in fillOrderImpl())
        .fee = jstod(aTrade.at("commission")),
        .feeAsset = aTrade.at("commissionAsset"),
        .latestTrade = 0,
        .tradeCount = 1,
    };
}


inline Fulfillment Fulfillment::fromStreamJson(const Json & aTrade)
{
    return {
        jstod(aTrade.at("l")),
        jstod(aTrade.at("Y")),
        jstod(aTrade.at("n")),
        aTrade.at("N"),
        aTrade.at("E"),
        1, // 1 trade
    };
}


std::ostream & operator<<(std::ostream & aOut, const Fulfillment & aFulfillment);


} // namespace tradebot
} // namespace ad
