#include "Order.h"

#include "Fulfillment.h"

#include "Logging.h"

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
        && isEqual(aLhs.amount, aRhs.amount)
        && isEqual(aLhs.fragmentsRate, aRhs.fragmentsRate)
        && aLhs.side == aRhs.side
        && aLhs.fulfillResponse == aRhs.fulfillResponse
        && aLhs.activationTime == aRhs.activationTime
        && aLhs.status == aRhs.status
        && aLhs.fulfillTime == aRhs.fulfillTime
        && isEqual(aLhs.executionRate, aRhs.executionRate)
        && isEqual(aLhs.takenHome, aRhs.takenHome)
        && isEqual(aLhs.commission, aRhs.commission)
        && aLhs.commissionAsset == aRhs.commissionAsset
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


Order & Order::resetAsInactive()
{
    activationTime = 0;
    status = Status::Inactive;
    fulfillTime = 0;
    executionRate = 0;
    commission = 0;
    commissionAsset.clear();
    takenHome = 0;
    exchangeId = -1;
    id = -1;

    return *this;
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


FulfilledOrder fulfill(Order & aOrder,
                       const Json & aQueryStatus,
                       const Fulfillment & aFulfillment)
{
    // Sanity check
    {
        if (! isEqual(aOrder.amount, jstod(aQueryStatus["executedQty"])))
        {
            spdlog::critical("Mismatched order '{}' amount and executed quantity: {} vs. {}.",
                             aOrder.getIdentity(),
                             aOrder.amount,
                             aQueryStatus["executedQty"]);
            throw std::logic_error("Mismatched original amount and executed quantity on order.");
        }

        if (! isEqual(aOrder.amount, aFulfillment.amountBase))
        {
            spdlog::critical("Mismatched order '{}' amount and accumulated fulfillment quantity: {} vs. {}.",
                             aOrder.getIdentity(),
                             aOrder.amount,
                             aFulfillment.amountBase);
            throw std::logic_error("Mismatched original amount and accumulated fulfillment quantity on order.");
        }
    }

    aOrder.status = Order::Status::Fulfilled;

    // Note: Initially, prefered to trust the global price reported in the query order Json,
    // maybe small rounding errors would accumulate when partial fills are involved.
    // Yet, it turns out that sometimes a limit order fulfills at a different price than the requested price,
    // (hopefully always at more adavantageous prices).
    // Since this difference might be very large compared to rounding erros,
    // now the accumulated fulfillment price is trusted over the global price.
    //aOrder.executionRate = jstod(aQueryStatus["price"]);
    aOrder.executionRate = aFulfillment.price();
    if (aOrder.executionRate > 0.)
    {
        Decimal globalPrice = jstod(aQueryStatus["price"]);
        if (globalPrice > 0.
            && (! isEqual(aOrder.executionRate, globalPrice)))
        {
            // Just warning, and use the fulfillment averaged price.
            spdlog::warn("Order '{}' global price {} is different from the fulfillment price {}, averaged from {} trade(s)."
                         " Use fulfillment price, difference is {}.",
                         aOrder.getIdentity(),
                         globalPrice,
                         aOrder.executionRate,
                         aFulfillment.tradeCount,
                         aOrder.executionRate - jstod(aQueryStatus["price"])
            );

        }
    }
    else
    {
        spdlog::critical("Cannot get the fulfillment price for order '{}'.",
                         aOrder.getIdentity());
        throw std::logic_error{"Cannot get its price while fulfilling an order."};
    }

    // In case of a market order, the "trade" response returns the fills (without any time attached)
    // and "transactTime". I suspect all fills are considered to have taken places at transaction time.
    // For other orders (limit), the times will be accumulated from the fills as they arrive on the websocket
    aOrder.fulfillTime = (aFulfillment.latestTrade ?
                          aFulfillment.latestTrade
                          : aQueryStatus.at("transactTime").get<MillisecondsSinceEpoch>());

    aOrder.commission = aFulfillment.fee;
    aOrder.commissionAsset = aFulfillment.feeAsset;

    return FulfilledOrder{aOrder};
}


} // namespace tradebot
} // namespace ad
