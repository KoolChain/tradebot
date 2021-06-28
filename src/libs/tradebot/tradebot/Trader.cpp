#include "Trader.h"

#include "Logging.h"

#include <boost/lexical_cast.hpp>


namespace ad {
namespace tradebot {

namespace detail {


    void filterAmountTickSize(Order & aOrder, SymbolFilters aFilters)
    {
        Decimal remainder;
        Decimal tickSize = aFilters.amount.tickSize;
        std::tie(aOrder.amount, remainder) = trade::computeTickFilter(aOrder.amount, tickSize);
        if (! isEqual(remainder, Decimal{0}))
        {
            spdlog::warn("Prepared order '{}' had to be filtered to respect tick size {}."
                         " Remainder is {}.",
                         aOrder.getIdentity(),
                         tickSize,
                         remainder);
        }
    }


    bool testAmountFilters(Order & aOrder, SymbolFilters aFilters)
    {
        if (! testAmount(aFilters, aOrder.amount, aOrder.fragmentsRate))
        {
            spdlog::info("Order '{}' (amount: {}, rate: {}, notional: {} {}) does not pass "
                         "amount filters (min: {}, max: {}, min notional: {}).",
                         aOrder.getIdentity(),
                         aOrder.amount,
                         aOrder.fragmentsRate,
                         aOrder.amount * aOrder.fragmentsRate,
                         aOrder.quote,
                         aFilters.amount.minimum,
                         aFilters.amount.maximum,
                         aFilters.minimumNotional);
            return false;
        }
        return true;
    }


} // namespace detail


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
                                             SymbolFilters aFilters)
{
    // Create the Inactive order in DB, assigning all matching fragments
    Order order = database.prepareOrder(name, aSide, aFragmentsRate, pair);
    detail::filterAmountTickSize(order, aFilters);
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
                                  const FulfilledOrder & aSpawningOrder)
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
                aSpawningOrder.base,
                aSpawningOrder.quote,
                accumulatedBaseAmount,
                rateEntry.first,
                reverse(aSpawningOrder.side),
                0, /* taken home */
                aSpawningOrder.id});
    }
    return result;
}


void Trader::spawnFragments(const FulfilledOrder & aOrder)
{
    SpawnMap spawnMap;

    for(Fragment & fragment : database.getFragmentsComposing(aOrder))
    {
        auto [spawns, takenHome] =
            spawner->computeResultingFragments(fragment, aOrder, database);

        fragment.takenHome = std::move(takenHome);
        database.update(fragment);

        spawnMap.appendFrom(fragment.id, spawns.begin(), spawns.end());
    }

    for (Fragment & newFragment : consolidate(spawnMap, aOrder))
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


SymbolFilters Trader::queryFilters()
{
   return exchange.queryFilters(pair);
}


bool Trader::completeFulfilledOrder(const FulfilledOrder & aFulfilledOrder)
{
    // Important: Everything happening on the database in this member function
    // is guarded as a single transaction.
    // This way, in case of crash, the order would not be marked fulfilled
    // and no fragments would have been spawned to the DB.
    // (and the whole process will restart from scratch on next launch)

    auto transaction = database.startTransaction();

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

    database.commit(std::move(transaction));
    return result;
}


bool Trader::completeOrder(Order & aOrder, const Fulfillment & aFulfillment)
{
    // Note: on 2021/06/11, there was a crash of NaiveBot with exception
    // "Mismatched original amount and executed quantity on order." in fulfill() below.
    // I suspect this was because the exchange returned an order-JSON that was not yet up to date,
    // and still at a partially filled state (the order had several trades).
    auto ensureFulfilled = [](const Json & aOrderJson)
    {
        return aOrderJson.at("status") == "FILLED";
    };

    std::optional<Json> orderJson = exchange.tryQueryOrder(aOrder, ensureFulfilled);
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


FulfilledOrder Trader::fillExistingMarketOrder(Order & aOrder)
{
    database.update(aOrder.setStatus(Order::Status::Sending));

    std::optional<FulfilledOrder> fulfilled;
    while(!(fulfilled = exchange.fillMarketOrder(aOrder)).has_value())
    {}

    completeFulfilledOrder(*fulfilled);
    return *fulfilled;
}


FulfilledOrder Trader::fillNewMarketOrder(Order & aOrder)
{
    database.insert(aOrder.resetAsInactive());
    return fillExistingMarketOrder(aOrder);
}


std::optional<FulfilledOrder> Trader::fillExistingLimitFokOrder(Order & aOrder,
                                                                Decimal aLimitRate,
                                                                Predicate & aPredicate)
{
    database.update(aOrder.setStatus(Order::Status::Sending));

    std::optional<FulfilledOrder> fulfilled;
    while(   aPredicate()
          && ! (fulfilled = exchange.fillLimitFokOrder(aOrder, aLimitRate)).has_value())
    {}

    if (fulfilled)
    {
        completeFulfilledOrder(*fulfilled);
    }
    else
    {
        spdlog::debug("Predicate interrupted limit FOK filling loop.");
        database.update(aOrder.setStatus(Order::Status::Inactive));
    }
    return fulfilled;
}


std::pair<std::size_t, std::size_t>
Trader::makeAndFillProfitableOrders(Interval aRateInterval,
                                    SymbolFilters aFilters,
                                    Predicate aPredicate)
{
    auto makeAndFill = [this, aFilters, &predicate = aPredicate]
                       (Side aSide, Decimal aRate) -> std::size_t
    {
        std::size_t counter = 0;
        for (const Decimal rate : database.getProfitableRates(aSide, aRate, pair))
        {
            Order order = database.prepareOrder(name, aSide, rate, pair);
            if (detail::testAmountFilters(order, aFilters))
            {
                detail::filterAmountTickSize(order, aFilters);
                if(! fillExistingLimitFokOrder(order, aRate, predicate))
                {
                    database.discardOrder(order);
                    break;
                }
                ++counter;
            }
            else
            {
                spdlog::warn("Order '{}' will not be placed because it does not pass amount filters.",
                             order.getIdentity());
                database.discardOrder(order);
            }
        }
        return counter;
    };

    auto sellCount = makeAndFill(Side::Sell, aRateInterval.front);
    auto buyCount  = makeAndFill(Side::Buy, aRateInterval.back);

    // Only log if there were matching orders to fill.
    spdlog::info("From rate interval [{}, {}] on {}, filled {} sell market orders and {} buy market orders.",
            aRateInterval.front,
            aRateInterval.back,
            pair.symbol(),
            sellCount,
            buyCount);

    return {sellCount, buyCount};
}


} // namespace tradebot
} // namespace ad
