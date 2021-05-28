#include "Trader.h"

namespace ad {
namespace tradebot {


Order Trader::placeOrderForMatchingFragments(Execution aExecution,
                                             Side aSide,
                                             Decimal aFragmentsRate,
                                             Order::FulfillResponse aFulfillResponse)
{
    // Create the Inactive order in DB, assigning all matching fragments
    Order order = database.prepareOrder(name, aSide, aFragmentsRate, pair, aFulfillResponse);

    // Send the order to the exchange
    database.update(order.setStatus(Order::Status::Sending));
    // Note: placeOrder() marks the order Active.
    database.update(exchange.placeOrder(order, aExecution));

    return order;
}


bool Trader::cancel(Order & aOrder)
{
    // TODO think about removing the "Cancelling" status.
    // Now I feel like it does not get us anything.
    database.update(aOrder.setStatus(Order::Status::Cancelling));
    bool result = exchange.cancelOrder(aOrder);

    if (result || (exchange.getOrderStatus(aOrder) != "FILLED"))
    {
        // TODO Handle partially filled orders properly instead of throwing
        Decimal partialFill = std::stod(exchange.queryOrder(aOrder)["executedQty"].get<std::string>());
        if (partialFill != 0.)
        {
            spdlog::critical("Order {} was cancelled but partially filled for {} {}.",
                             static_cast<const std:: string &>(aOrder.clientId()),
                             partialFill,
                             aOrder.base);
            throw std::logic_error("Cannot handle partially filled orders.");
        }

        // There is little benefit in marking it inactive just before removal:
        // The program might crash just before this line, and it should still be able to catch up next run.
        // (Adding a different "potential crash point" just after this line, before discarding from DB,
        //  does not buy us anything).
        // Yet it is conveniently updating the in/out aOrder, helping the calling site keep track of state.
        database.update(aOrder.setStatus(Order::Status::Inactive));

        database.discardOrder(aOrder);
    }
    {

    }

    return result;
}


} // namespace tradebot
} // namespace ad
