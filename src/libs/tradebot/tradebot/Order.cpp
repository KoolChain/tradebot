#include "Order.h"

#include <spdlog/spdlog.h>

#include <ostream>


namespace ad {
namespace tradebot {


std::string Order::symbol() const
{
    return base + quote;
}


binance::ClientId Order::clientId() const
{
    // TODO Ideally we would have separate types to model orders matched in DB or not.
    // Right now we rely on runtime checks.
    if (id == -1)
    {
        throw std::logic_error{"Cannot retrieve the client id for an order not matched in database."};
    }
    return binance::ClientId{traderName + '-' + std::to_string(id)};
}


bool operator==(const Order & aLhs, const Order & aRhs)
{
    return
        aLhs.traderName == aRhs.traderName
        && aLhs.base == aRhs.base
        && aLhs.quote == aRhs.quote
        && aLhs.amount == aRhs.amount
        && aLhs.fragmentsRate == aRhs.fragmentsRate
        && aLhs.executionRate== aRhs.executionRate
        && aLhs.side == aRhs.side
        && aLhs.fulfillResponse == aRhs.fulfillResponse
        && aLhs.activationTime == aRhs.activationTime
        && aLhs.status == aRhs.status
        && aLhs.takenHome == aRhs.takenHome
        && aLhs.fulfillTime == aRhs.fulfillTime
        && aLhs.exchangeId == aRhs.exchangeId
        ;
}


bool operator!=(const Order & aLhs, const Order & aRhs)
{
    return ! (aLhs == aRhs);
}


std::ostream & operator<<(std::ostream & aOut, const Order & aRhs)
{
    return aOut
        << aRhs.status << " order " << static_cast<const std::string &>(aRhs.clientId()) << ' '
        << std::string(aRhs.side == Side::Sell ? "Sell" : "Buy")
        << ' ' << aRhs.amount << ' ' << aRhs.base
        << " (fragments: " << aRhs.fragmentsRate
        << " execution: " << aRhs.executionRate << ' '
        << aRhs.quote << " per " << aRhs.base << ")"
        ;
}


struct Fulfill
{
    Decimal amountBase{0};
    Decimal amountQuote{0};
    Decimal fee{0};
    std::string feeAsset;
    MillisecondsSinceEpoch latestTrade{0};

    Decimal price() const
    {
        return amountQuote / amountBase;
    }
};


template <class T_tradeIterator>
Fulfill analyzeTrades(T_tradeIterator aBegin, const T_tradeIterator aEnd, const Order & aOrder)
{
    Fulfill result;
    for (; aBegin != aEnd; ++aBegin)
    {
        auto fill = *aBegin;
        result.amountBase += jstod(fill["qty"]);
        result.amountQuote += jstod(fill["price"]) * jstod(fill["qty"]);
        result.fee += jstod(fill["commission"]);
        if (result.feeAsset == "")
        {
            result.feeAsset = fill["commissionAsset"];
        }
        else if (result.feeAsset != fill["commissionAsset"])
        {
            spdlog::critical("Inconsistent commissions assets {} vs. {} in fills for order '{}'.",
                             result.feeAsset,
                             fill["commissionAsset"],
                             static_cast<const std::string &>(aOrder.clientId())
                             );
            throw std::logic_error{"Different fills for the same order have different commissions assets."};
        }
        if (fill.contains("transactionTime"))
        {
            result.latestTrade = std::max<MillisecondsSinceEpoch>(result.latestTrade,
                                                                  fill["transactionTime"]);
        }
    }
    return result;
}


FulfilledOrder fulfillFromQuery(const Order & aOrder, const Json & aQueryStatus)
{
    FulfilledOrder result{aOrder};

    // Sanity check
    {
        if (aOrder.amount != jstod(aQueryStatus["executedQty"]))
        {
            spdlog::critical("Mismatched order amount and executed quantity: {} vs. {}.",
                             aOrder.amount,
                             aQueryStatus["executedQty"]);
            throw std::logic_error("Mismatched original amount and executed quantity on order");
        }

        if(! aQueryStatus.contains("fills"))
        {
            spdlog::critical("'Fills' array is absent from Json for order '{}'.",
                             static_cast<const std::string &>(aOrder.clientId()));
            throw std::logic_error("Json status for orders must contain a 'fills' array.");
        }
    }

    Fulfill fulfill =
        analyzeTrades(aQueryStatus["fills"].begin(), aQueryStatus["fills"].end(), aOrder);

    result.status = Order::Status::Fulfilled;
    result.executionRate = jstod(aQueryStatus["price"]);
    if (result.executionRate > 0.)
    {
        if (result.executionRate != fulfill.price())
        {
            // Just warning, and use the order global price.
            spdlog::warn("The order global price {} is different from the price averaged from trades {}.",
                         result.executionRate,
                         fulfill.price());

        }
    }
    else if (fulfill.price() > 0.)
    {
        result.executionRate = fulfill.price();
    }
    else
    {
        spdlog::critical("Cannot get the fulfill price for order '{}'.",
                         static_cast<const std::string &>(aOrder.clientId()));
        throw std::logic_error{"Cannot get the price while fulfilling an order."};
    }

    // Important 2021/05/28: I cannot find a way to get the different "FILLS" time for market orders.
    // Uses the <strike>last time order was updated if available (not sure how accurate that is)</strike>
    // transaction time instead.
    // In case of a market order, the "trade" response returns the fills (without any time attached)
    // and "transactTime". I suspect all fills are considered to have taken places at transaction time.
    // For other orders (limit), the times will be accumulated from the fills as they arrive on the websocket
    result.fulfillTime = (fulfill.latestTrade ?
                          fulfill.latestTrade
                          : aQueryStatus.at("transactTime").get<MillisecondsSinceEpoch>());

    return result;
}


} // namespace tradebot
} // namespace ad
