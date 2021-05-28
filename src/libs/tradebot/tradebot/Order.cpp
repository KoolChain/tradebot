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
            throw std::logic_error("Mismatching original amount and executed quantity on order");
        }
    }

    if (jstod(aQueryStatus["price"]) == 0.)
    {
        spdlog::critical("Cannot handle market order offline completion at the moment, "
                         "which is the case for '{}'.",
                         static_cast<const std::string &>(aOrder.clientId()) );
        throw std::logic_error("Cannot handle market order offline completion.");
    }
    // TODO move somewhere appropriate (when creating market orders for example)
    // it cannot be used here, because it is not possible to retrieve the list of fills
    // "after the fact" (i.e. after the order creation)
    // see: https://dev.binance.vision/t/finding-the-price-for-market-orders/201/8?u=shredastaire
    //Decimal filledPrice = 0.;
    //Decimal fee = 0.;
    //std::string feeAsset;
    //for (const auto & fill : aQueryStatus["fills"])
    //{
    //    filledPrice += jstod(fill["price"]) * jstod(fill["qty"]);
    //    fee += jstod(fill["commission"]);
    //    if (feeAsset == "")
    //    {
    //        feeAsset = fill["commissionAsset"];
    //    }
    //    else if (feeAsset != fill["commissionAsset"])
    //    {
    //        spdlog::critical("Inconsistent commissions assets {} vs. {} in fills: '{}'",
    //                         feeAsset,
    //                         fill["commissionAsset"],
    //                         aQueryStatus["fills"].dump()); // single line json dump
    //        throw std::logic_error{"Different fills for the same order have different commissions assets."};
    //    }
    //}

    result.status = Order::Status::Fulfilled;
    result.executionRate = jstod(aQueryStatus["price"]);
    // Important 2021/05/28: I cannot find a way to get the different "FILLS" time.
    // Uses the last time order was updated instead (not sure how accurate that is).
    result.fulfillTime = aQueryStatus["updateTime"];

    return result;
}


} // namespace tradebot
} // namespace ad
