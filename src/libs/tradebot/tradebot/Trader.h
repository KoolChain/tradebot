#pragma once


#include "Database.h"
#include "Exchange.h"
#include "Spawner.h"
#include "SymbolFilters.h"

#include <trademath/Interval.h>


namespace ad {
namespace tradebot {


struct Trader
{
    using Predicate = std::function<bool(void)>;

private:
    void sendExistingOrder(Execution aExecution, Order & aOrder);
    FulfilledOrder fillExistingMarketOrder(Order & aOrder);
    FulfilledOrder fillExistingMarketOrder(Order && aOrder)
    {
        return fillExistingMarketOrder(aOrder);
    }
    std::optional<FulfilledOrder> fillExistingLimitFokOrder(Order & aOrder, Predicate & aPredicate);

public: // should be private, but requires testing
    bool completeFulfilledOrder(const FulfilledOrder & aFulfilledOrder);

public:
    void placeNewOrder(Execution aExecution, Order & aOrder);

    Order placeOrderForMatchingFragments(Execution aExecution,
                                         Side aSide,
                                         Decimal aFragmentsRate,
                                         SymbolFilters aFilters = SymbolFilters{});

    /// \brief Attempt to cancel the order on the exchange,
    /// and always discard the order from the database.
    bool cancel(Order & aOrder);

    /// \brief Cancel all orders, and remove them from the database.
    /// \return The number of orders that were actually cancelled on the exchange.
    /// This might differ from the number of orders which are removed from the database.
    int cancelLiveOrders();


    /// \brief Spawn all counter-fragments required by the completion of `aOrder`.
    ///
    /// It takes care of spawning from each fragment composing aOrder, while recording their
    /// taken home. The spawning policy is controlled via `spawner` data member.
    /// Any proposed fragment with a null or negative amount will be discarded.
    ///
    /// \important: A given order is only allowed to spawn a single fragment per target rate.
    /// (It can spawns an arbitrary number of fragments in total, just one max per target rate.)
    /// This is imposed to avoid potential explosion in number of spawned fragments while the coin prices
    /// might cycle between the same target rates.
    /// To follow this requirement, this member function will consolidate all the spawns from different
    /// fragments targeting the same rate into a single resulting Fragment stored in database.
    void spawnFragments(const FulfilledOrder & aOrder);

    SymbolFilters queryFilters();

    //
    // High-level API
    //
    void cleanup();

    /// \brief Will place new market orders until it fulfills (by oposition to expiring).
    FulfilledOrder fillNewMarketOrder(Order & aOrder);

    /// \brief High-level operation that create all orders that would be profitable
    /// at rates in `aInterval`, and fill them as limit Fill Or Kill.
    ///
    /// Will make separate orders for each available fragment rate on a given side:
    /// * one sell order per sell fragment rate below the interval lower bound.
    /// * one buy order per buy fragment rate above the interval higher bound.
    ///
    /// \param aPredicate A function called before each attempt to place an order, that will
    /// interrupt the procedure if it returns false.
    ///
    /// \return A pair containing the number of filled sell orders and buy orders.
    std::pair<std::size_t /*filled sell*/, std::size_t /*filled buy*/>
    makeAndFillProfitableOrders(Interval aInterval,
                                SymbolFilters aFilters,
                                Predicate aPredicate = [](){return true;});

    /// \brief To be called when an order did complete on the exchange, with its already accumulated
    /// fulfillment.
    ///
    /// \return `true` if the order was completed by this invocation, `false` otherwise.
    /// (It might be false if the order was already recorded as completed.)
    bool completeOrder(Order & aOrder, const Fulfillment & aFulfillment);

    std::string name;
    Pair pair;
    Database database;
    Exchange exchange;
    std::unique_ptr<SpawnerBase> spawner{std::make_unique<NullSpawner>()};
};


} // namespace tradebot
} // namespace ad
