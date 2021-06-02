#include "Order.h"

#include "Fulfillment.h"

#include <spdlog/spdlog.h>

#include <ostream>
#include <sstream>


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


std::string Order::getIdentity() const
{
    std::ostringstream oss;
    oss << static_cast<const std::string &>(clientId())
        << ' ' << base << quote
        << ", exchange id: " << exchangeId
        ;
    return oss.str();
}


FulfilledOrder fulfill(const Order & aOrder,
                       const Json & aQueryStatus,
                       const Fulfillment & aFulfillment)
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
    }

    result.status = Order::Status::Fulfilled;

    result.executionRate = jstod(aQueryStatus["price"]);
    if (result.executionRate > 0.)
    {
        if (result.executionRate != aFulfillment.price())
        {
            // Just warning, and use the order global price.
            spdlog::warn("The order global price {} is different from the price averaged from trades {}.",
                         result.executionRate,
                         aFulfillment.price());

        }
    }
    else if (aFulfillment.price() > 0.)
    {
        result.executionRate = aFulfillment.price();
    }
    else
    {
        spdlog::critical("Cannot get the aFulfillment price for order '{}'.",
                         aOrder.getIdentity());
        throw std::logic_error{"Cannot get the price while aFulfillmenting an order."};
    }

    // In case of a market order, the "trade" response returns the fills (without any time attached)
    // and "transactTime". I suspect all fills are considered to have taken places at transaction time.
    // For other orders (limit), the times will be accumulated from the fills as they arrive on the websocket
    result.fulfillTime = (aFulfillment.latestTrade ?
                          aFulfillment.latestTrade
                          : aQueryStatus.at("transactTime").get<MillisecondsSinceEpoch>());

    return result;
}


} // namespace tradebot
} // namespace ad
