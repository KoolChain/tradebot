#include "Trader.h"

#include <boost/lexical_cast.hpp>


namespace ad {
namespace tradebot {


void Trader::sendExistingOrder(Execution aExecution, Order & aOrder)
{
    // Send the order to the exchange
    database.update(aOrder.setStatus(Order::Status::Sending));
    // Note: placeOrder() marks the order Active.
    database.update(exchange.placeOrder(aOrder, aExecution));
}



void Trader::placeNewOrder(Execution aExecution, Order & aOrder)
{
    // Insert a new order in database, after reseting all "non-client" data members at defaults.
    database.insert(aOrder.resetAsInactive());
    sendExistingOrder(aExecution, aOrder);
}


Order Trader::placeOrderForMatchingFragments(Execution aExecution,
                                             Side aSide,
                                             Decimal aFragmentsRate,
                                             Order::FulfillResponse aFulfillResponse)
{
    // Create the Inactive order in DB, assigning all matching fragments
    Order order = database.prepareOrder(name, aSide, aFragmentsRate, pair, aFulfillResponse);
    sendExistingOrder(aExecution, order);
    return order;
}


bool Trader::cancel(Order & aOrder)
{
    // TODO think about removing the "Cancelling" status.
    // Now I feel like it does not get us anything.
    database.update(aOrder.setStatus(Order::Status::Cancelling));
    bool result = exchange.cancelOrder(aOrder);

    std::optional<Json> orderJson = exchange.tryQueryOrder(aOrder);
    const std::string status = (orderJson ? orderJson->at("status") : "NOTEXISTING");
    spdlog::debug("Order '{}' is being cancelled, current status on the exchange: {}.",
                  aOrder.getIdentity(),
                  status);

    // The order was received by the exchange, but the exchange ID was not recorded back
    // (i.e. a 'Sending' order).
    // Note that all orders are marked 'Cancelling' from this point, don't assert it to be 'Sending'.
    if (orderJson && aOrder.exchangeId == -1)
    {
        aOrder.exchangeId = orderJson->at("orderId");
        database.update(aOrder);
        spdlog::debug("Filled-in exchange id for order '{}'.", aOrder.getIdentity());
    }

    if (status != "FILLED")
    {
        // TODO Handle partially filled orders properly instead of throwing
        if (status != "NOTEXISTING")
        {
            Decimal partialFill = jstod(orderJson->at("executedQty"));
            if (partialFill != 0.)
            {
                spdlog::critical("Order {} was cancelled but partially filled for {}/{} {}.",
                                 static_cast<const std:: string &>(aOrder.clientId()),
                                 partialFill,
                                 aOrder.amount,
                                 aOrder.base);
                throw std::logic_error("Cannot handle partially filled orders.");
            }
        }

        // There is little benefit in marking it inactive just before removal:
        // The program might crash just before this line, and it should still be able to catch up next run.
        // (Adding a different "potential crash point" just after this line, before discarding from DB,
        //  does not buy us anything).
        // Yet it is conveniently updating the in/out aOrder, helping the calling site keep track of state.
        database.update(aOrder.setStatus(Order::Status::Inactive));

        database.discardOrder(aOrder);
    }
    else
    {
        // I exected that it would not be possible to have partial fills when status is "FILLED"
        // but I think I observed the opposite during some runs of the tests.

        Decimal executed = jstod(orderJson->at("executedQty"));
        if (executed != aOrder.amount)
        {
            spdlog::critical("Order '{}' was marked 'FILLED' but partially filled for {}/{} {}.",
                             aOrder.getIdentity(),
                             executed,
                             aOrder.amount,
                             aOrder.base);
            throw std::logic_error("Does not expect 'FILLED' order to be partially filled.");
        }

        // The order completely filled
        completeOrder(aOrder, exchange.accumulateTradesFor(aOrder));
    }

    return result;
}


struct SpawnMap
{
    template <class T_iterator>
    void appendFrom(FragmentId aParent, T_iterator aBegin, const T_iterator aEnd)
    {
        for(; aBegin != aEnd; ++aBegin)
        {
            data[aBegin->rate].emplace_back(aParent, aBegin->base);
        }
    }

    std::map<
        Decimal /*target price*/,
        std::vector<
            std::pair<FragmentId /*parent*/, Decimal /*spawned base amount*/>
        >
    > data;
};


std::vector<Fragment> consolidate(const SpawnMap & aSpawnMap,
                                  Pair aPair,
                                  Side aSide)
{
    std::vector<Fragment> result;
    for (const auto & rateEntry : aSpawnMap.data)
    {
        Decimal accumulatedBaseAmount = std::accumulate(
                rateEntry.second.begin(), rateEntry.second.end(),
                Decimal{0},
                [](const Decimal & acc, const auto & pair)
                {
                    return acc + pair.second;
                });
        result.push_back(Fragment{
                aPair.base,
                aPair.quote,
                accumulatedBaseAmount,
                rateEntry.first,
                aSide});
    }
    return result;
}


void Trader::spawnFragments(const FulfilledOrder & aOrder)
{
    // TODO Everything happening on the database in this member function
    // should be guarded as a single transaction.

    SpawnMap spawnMap;

    for(Fragment & fragment : database.getFragmentsComposing(aOrder))
    {
        auto [resultingFragments, takenHome] =
            spawner->computeResultingFragments(fragment, aOrder, database);

        fragment.takenHome = std::move(takenHome);
        database.update(fragment);

        spawnMap.appendFrom(fragment.id, resultingFragments.begin(), resultingFragments.end());
    }

    for (Fragment & newFragment : consolidate(spawnMap,
                                              aOrder.pair(),
                                              reverse(aOrder.side)))
    {
        // Use isEqual to remove rounding errors that would make it just above zero
        // and also discard invalid negative amounts, in case they arise.
        if (! isEqual(newFragment.amount, 0) && newFragment.amount > 0)
        {
            database.insert(newFragment);
        }
        else
        {
            spdlog::warn("Spawning fragments for order '{}' proposed a fragment with invalid amount {}."
                          " Ignoring it.",
                          aOrder.getIdentity(),
                          newFragment.amount);
        }
    }
}


bool Trader::completeFulfilledOrder(const FulfilledOrder & aFulfilledOrder)
{
    bool result = database.onFillOrder(aFulfilledOrder);
    if (result)
    {
        spdlog::info("Recorded completion of {} order '{}' for {} {} at a price of {} {}.",
                boost::lexical_cast<std::string>(aFulfilledOrder.side),
                aFulfilledOrder.getIdentity(),
                aFulfilledOrder.amount,
                aFulfilledOrder.base,
                aFulfilledOrder.executionRate,
                aFulfilledOrder.quote
        );

        spawnFragments(aFulfilledOrder);
    }
    return result;
}


bool Trader::completeOrder(Order & aOrder, const Fulfillment & aFulfillment)
{
    std::optional<Json> orderJson = exchange.tryQueryOrder(aOrder);
    if (orderJson)
    {
        return completeFulfilledOrder(fulfill(aOrder, *orderJson, aFulfillment));
    }
    else
    {
        spdlog::critical("Unable to complete order '{}', as it cannot be queried on the exchange.",
                         aOrder.getIdentity());
        throw std::logic_error("Order could not be queried on the exchange,"
                               " it cannot be completed.");
    }
}


int Trader::cancelLiveOrders()
{
    auto cancelForStatus = [&](Order::Status status)
    {
        auto orders = database.selectOrders(pair, status);
        spdlog::info("Found {} {} order(s).", orders.size(), boost::lexical_cast<std::string>(status));
        return std::count_if(orders.begin(), orders.end(), [&](Order & order){return cancel(order);});
    };

    return
        cancelForStatus(Order::Status::Active)
        + cancelForStatus(Order::Status::Cancelling)
        + cancelForStatus(Order::Status::Sending);
}


void Trader::cleanup()
{
    // Clean-up inactive orders
    auto inactiveOrders = database.selectOrders(pair, Order::Status::Inactive);
    spdlog::info("Found {} Inactive order(s).", inactiveOrders.size());
    for (auto inactive : inactiveOrders)
    {
        database.discardOrder(inactive);
    }
    // Cancel all other orders which are not marked fulfilled
    cancelLiveOrders();
}


FulfilledOrder Trader::fillNewMarketOrder(Order & aOrder)
{
    // No need to go through the Inactive state
    database.insert(aOrder.resetAsInactive().setStatus(Order::Status::Sending));

    std::optional<FulfilledOrder> fulfilled;
    while(!(fulfilled = exchange.fillMarketOrder(aOrder)).has_value())
    {}

    completeFulfilledOrder(*fulfilled);
    return *fulfilled;
}


} // namespace tradebot
} // namespace ad
