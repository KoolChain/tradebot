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
    aOrder.status = Order::Status::Inactive;
    database.insert(aOrder);
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

    if (const std::string status = exchange.getOrderStatus(aOrder); status != "FILLED")
    {
        // TODO Handle partially filled orders properly instead of throwing
        if (status != "NOTEXISTING")
        {
            Decimal partialFill = jstod(exchange.queryOrder(aOrder)["executedQty"]);
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

        Decimal executed = jstod(exchange.queryOrder(aOrder)["executedQty"]);
        if (executed != aOrder.amount)
        {
            spdlog::critical("Order {} was marked 'FILLED' but partially filled for {}/{} {}.",
                             static_cast<const std:: string &>(aOrder.clientId()),
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


bool Trader::completeOrder(const Order & aOrder, const Fulfillment & aFulfillment)
{
    Json orderJson = exchange.queryOrder(aOrder);
    bool result = database.onFillOrder(fulfill(aOrder, orderJson, aFulfillment));
    if (result)
    {
        spdlog::info("Recorded completion of {} order '{}' for {} {} at a price of {} {}.",
                boost::lexical_cast<std::string>(aOrder.side),
                aOrder.getIdentity(),
                aOrder.amount,
                aOrder.base,
                aOrder.executionRate,
                aOrder.quote
        );
    }
    return result;
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


} // namespace tradebot
} // namespace ad
