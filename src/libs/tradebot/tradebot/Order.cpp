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


Pair Order::pair() const
{
    return {base, quote};
}


binance::ClientId Order::clientId() const
{
    // TODO Ideally we would have separate types to model orders matched in DB or not.
    // Right now we rely on runtime checks.
    if (id == -1)
    {
        spdlog::critical("Cannot retrieve the client id for an order ({} {})"
                          " which is not matched in database.",
                          baseAmount,
                          symbol());
        throw std::logic_error{"Cannot retrieve the client id for an order not matched in database."};
    }
    return binance::ClientId{traderName + '-' + std::to_string(id)};
}


Decimal Order::executionQuoteAmount() const
{
    if (status != Status::Fulfilled)
    {
        spdlog::critical("Cannot get execution quote amount for order '{}' since it is not fulfilled.",
                          getIdentity());
        throw std::logic_error{"Cannot get execution quote amount for an order which is not fulfilled."};
    }
    return baseAmount * executionRate;
}


bool operator==(const Order & aLhs, const Order & aRhs)
{
    return
        aLhs.traderName == aRhs.traderName
        && aLhs.base == aRhs.base
        && aLhs.quote == aRhs.quote
        && isEqual(aLhs.baseAmount, aRhs.baseAmount)
        && isEqual(aLhs.fragmentsRate, aRhs.fragmentsRate)
        && aLhs.side == aRhs.side
        && aLhs.activationTime == aRhs.activationTime
        && aLhs.status == aRhs.status
        && aLhs.fulfillTime == aRhs.fulfillTime
        && isEqual(aLhs.executionRate, aRhs.executionRate)
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
        << ' ' << aRhs.baseAmount << ' ' << aRhs.base
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
        if (! isEqual(aOrder.baseAmount, jstod(aQueryStatus["executedQty"])))
        {
            spdlog::critical("Mismatched order '{}' amount and executed quantity: {} vs. {}.",
                             aOrder.getIdentity(),
                             aOrder.baseAmount,
                             aQueryStatus["executedQty"]);
            throw std::logic_error("Mismatched original amount and executed quantity on order.");
        }

        if (! isEqual(aOrder.baseAmount, aFulfillment.amountBase))
        {
            spdlog::critical("Mismatched order '{}' amount and accumulated fulfillment quantity: {} vs. {}.",
                             aOrder.getIdentity(),
                             aOrder.baseAmount,
                             aFulfillment.amountBase);
            throw std::logic_error("Mismatched original amount and accumulated fulfillment quantity on order.");
        }

        if (aQueryStatus.at("status") != "FILLED")
        {
            spdlog::critical("Provided query status for order '{}' indicates status {}, should be \"FILLED\".",
                             aOrder.getIdentity(),
                             aQueryStatus.at("status"));
            throw std::logic_error("Unexpected order status in provided query status json.");
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
            Decimal difference = aOrder.executionRate - jstod(aQueryStatus["price"]);
            // Just warning, and use the fulfillment averaged price.
            // NOTE Ad 2023/12/21: Sadly, the average price is likely to be a rational number without finite decimal representation.
            // Even though each individual fill seems to respect the exchange filters, sum(quoteQty)/sum(baseQty) is not constrained.
            spdlog::log(   (aOrder.side == Side::Sell && difference > 0)
                        || (aOrder.side == Side::Buy  && difference < 0) ? spdlog::level::info
                                                                         : spdlog::level::warn,
                        "{} order '{}' global price {} is different from the fulfillment price {}, averaged from {} trade(s)."
                        " Use fulfillment price, difference is {}.",
                        boost::lexical_cast<std::string>(aOrder.side),
                        aOrder.getIdentity(),
                        globalPrice,
                        aOrder.executionRate,
                        aFulfillment.tradeCount,
                        difference);
        }
    }
    else
    {
        spdlog::critical("Cannot get the fulfillment price for order '{}'.",
                         aOrder.getIdentity());
        throw std::logic_error{"Cannot get its price while fulfilling an order."};
    }

    // Warn if the order executed at a "loss" compared to a set fragments rate
    if ( aOrder.fragmentsRate // only check if a fragments rate was explicitly set
         && (   (aOrder.side == Side::Sell && aOrder.executionRate < aOrder.fragmentsRate)
             || (aOrder.side == Side::Buy && aOrder.executionRate > aOrder.fragmentsRate)))
    {
        spdlog::error("{} order '{}' fragment rate is set at {}, but it executed at {}.",
                boost::lexical_cast<std::string>(aOrder.side),
                aOrder.getIdentity(),
                aOrder.fragmentsRate,
                aOrder.executionRate);
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
