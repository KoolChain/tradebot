#pragma once


#include <tradebot/Fulfillment.h>
#include <tradebot/Logging.h>
#include <tradebot/Order.h>
#include <tradebot/Trader.h>

#include <trademath/FilterUtilities.h>

#include <boost/asio/post.hpp>
#include <boost/asio/deadline_timer.hpp>


namespace ad {

struct OrderTracker
{
    tradebot::Order order;;
    tradebot::Fulfillment fulfillment;
};


static std::chrono::minutes gMaxFulfillPeriod{3};

struct NaiveBot
{
    tradebot::Trader trader;
    std::optional<OrderTracker> currentOrder;
    double percentage;
    boost::asio::io_context mainLoop;
    boost::asio::steady_timer orderTimer{mainLoop, gMaxFulfillPeriod};

    void onPartialFill(Json aReport);

    void onCompletion(Json aReport);

    void onReport(Json aExecutionReport);

    void onOrderTimer(const boost::system::error_code & aErrorCode);

    void startEventLoop();

    void run();
};


inline void NaiveBot::onPartialFill(Json aReport)
{
    // Sanity check
    if (aReport.at("c") != currentOrder->order.clientId())
    {
        spdlog::critical("Trying to accumulate a trade report for order {} with order '{}'.",
                         aReport.at("c"),
                         currentOrder->order.getIdentity());
        throw std::logic_error{"Trying to accumulate trades of different orders."};
    }

    currentOrder->fulfillment.accumulate(
            tradebot::Fulfillment::fromStreamJson(aReport),
            currentOrder->order);
}


tradebot::Order & transformOrder(tradebot::Order & aOrder, double aPercentage)
{
    aOrder.reverseSide();
    if (aOrder.side == tradebot::Side::Buy)
    {
        aOrder.fragmentsRate = trade::applyTickSize((1.0 - aPercentage) * aOrder.executionRate);
    }
    else
    {
        aOrder.fragmentsRate = trade::applyTickSize((1.0 + aPercentage) * aOrder.executionRate);
    }
    return aOrder;
}


inline void NaiveBot::onCompletion(Json aReport)
{
    spdlog::trace("Completion report handler for order {}, current order: '{}'.",
                  aReport.at("c"),
                  currentOrder->order.getIdentity());

    // Complete the order
    onPartialFill(aReport);

    // 'z' is cumulative filled quantity
    if (jstod(aReport.at("z")) != currentOrder->order.amount)
    {
        spdlog::critical("Order '{}' has missmatching amount vs. reported cumulative filled quantity: {} vs. {}.",
            currentOrder->order.getIdentity(),
            currentOrder->order.amount,
            jstod(aReport.at("z")));
        throw std::logic_error{"Mismatched order amount vs. reported cumulative filled quantity."};

    }
    // TODO: add attempt to retrieves all trades for the order (might have missed some partially_filled reports)
    // to make it a bit more robust to network errors, 24h reconnections, etc.
    if (currentOrder->fulfillment.amountBase != currentOrder->order.amount)
    {
        spdlog::critical("Order '{}' has missmatching amount vs. accumulated fulfillment: {} vs. {}.",
            currentOrder->order.getIdentity(),
            currentOrder->order.amount,
            currentOrder->fulfillment.amountBase);
        throw std::logic_error{"Mismatched order amount vs. accumulated fulfillment."};
    }

    trader.completeOrder(currentOrder->order, currentOrder->fulfillment);

    // Place the next order
    currentOrder.emplace(OrderTracker{
            tradebot::Order{transformOrder(currentOrder->order, percentage)},
            tradebot::Fulfillment{}
    });
    trader.placeNewOrder(tradebot::Execution::Limit, currentOrder->order);
    orderTimer.expires_after(gMaxFulfillPeriod);
}


inline void NaiveBot::onOrderTimer(const boost::system::error_code & aErrorCode)
{
    if (aErrorCode == boost::asio::error::operation_aborted)
    {
        // This is normal when the timer expiration is explicitly reset after placing an order.
        spdlog::trace("Order timer reset.");
    }
    else if (aErrorCode)
    {
        spdlog::error("Interruption of order timer: {}.", aErrorCode.message());
    }
    else
    {
        spdlog::trace("Order timer event.");

        // Might still be a race with the exchange
        if (currentOrder->fulfillment.tradeCount == 0)
        {
            trader.cancel(currentOrder->order);
            trader.fillNewMarketOrder(currentOrder->order);
        }
        else
        {
            spdlog::warn("Order timer fired, but there are already trades in the fulfillment. Keep on waiting.");
        }

        // It is required to set expiration in the future before calling wait, otherwise the
        // callback would be immediatly queued again.
        // Note: It does not really matter which duration is put here in case of normal timer event:
        // the market order completion report will reset the timer.
        orderTimer.expires_after(gMaxFulfillPeriod);
    }

    orderTimer.async_wait(std::bind(&NaiveBot::onOrderTimer, this, std::placeholders::_1));
}


inline void NaiveBot::onReport(Json aExecutionReport)
{
    if (aExecutionReport.at("c") == currentOrder->order.clientId())
    {
        if (aExecutionReport.at("X") == "PARTIALLY_FILLED")
        {
            onPartialFill(aExecutionReport);
        }
        else if (aExecutionReport.at("X") == "FILLED")
        {
            onCompletion(aExecutionReport);
        }
        else if (aExecutionReport.at("X") == "NEW")
        {
            spdlog::info("Exchange accepted new order '{}'.", aExecutionReport.at("c"));
        }
    }
    else
    {
        spdlog::info("Saw a report for external order {} with status {}.",
                aExecutionReport.at("c"),
                aExecutionReport.at("X"));
    }
}


inline void NaiveBot::startEventLoop()
{
    // io_context run returns as soon as there is no work left.
    // create a timer never expiring, so it keeps on waiting for events to post work.
    boost::asio::deadline_timer runForever{mainLoop, boost::posix_time::pos_infin};
    runForever.async_wait([](const boost::system::error_code & aErrorCode)
        {
            spdlog::error("Interruption of run forever timer: {}.", aErrorCode.message());
        }
    );

    mainLoop.run();
}


inline void NaiveBot::run()
{
    // Initial cleanup
    trader.cleanup();

    trader.exchange.openUserStream([this](Json aExecutionReport)
    {
        boost::asio::post(mainLoop, std::bind(&NaiveBot::onReport, this, aExecutionReport));
    });
    trader.fillNewMarketOrder(currentOrder->order);

    orderTimer.async_wait(std::bind(&NaiveBot::onOrderTimer, this, std::placeholders::_1));

    startEventLoop();
}


} // namespace ad
