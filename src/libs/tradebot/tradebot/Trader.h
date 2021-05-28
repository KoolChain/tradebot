#pragma once


#include "Database.h"
#include "Exchange.h"


namespace ad {
namespace tradebot {


struct Trader
{
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

    std::string name;
    Pair pair;
    Database database;
    Exchange exchange;
};


} // namespace tradebot
} // namespace ad
