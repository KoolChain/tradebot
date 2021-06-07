#pragma once


#include "Database.h"
#include "Exchange.h"


namespace ad {
namespace tradebot {


struct Trader
{
private:
    void sendExistingOrder(Execution aExecution, Order & aOrder);

    bool completeFulfilledOrder(const FulfilledOrder & aFulfilledOrder);

public:
    void placeNewOrder(Execution aExecution, Order & aOrder);

    Order placeOrderForMatchingFragments(Execution aExecution,
                                         Side aSide,
                                         Decimal aFragmentsRate,
                                         Order::FulfillResponse aFulfillResponse);

    /// \brief Attempt to cancel the order on the exchange,
    /// and always discard the order from the database.
    bool cancel(Order & aOrder);

    /// \brief Cancel all orders, and remove them from the database.
    /// \return The number of orders that were actually cancelled on the exchange.
    /// This might differ from the number of orders which are removed from the database.
    int cancelLiveOrders();

    /// \brief To be called when an order did complete on the exchange, with its already accumulated
    /// fulfillment.
    ///
    /// \return `true` if the order was completed by this invocation, `false` otherwise.
    /// (It might be false if the order was already recorded as completed.)
    bool completeOrder(Order & aOrder, const Fulfillment & aFulfillment);

    //
    // High-level API
    //
    void cleanup();

    /// \brief Will place new market orders until it fulfills (by oposition to expiring).
    FulfilledOrder fillNewMarketOrder(Order & aOrder);


    std::string name;
    Pair pair;
    Database database;
    Exchange exchange;
};


} // namespace tradebot
} // namespace ad
