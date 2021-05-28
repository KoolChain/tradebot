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

    std::string name;
    Pair pair;
    Database database;
    Exchange exchange;
};


} // namespace tradebot
} // namespace ad
