#pragma once


#include<tradebot/Fulfillment.h>
#include<tradebot/Order.h>
#include<tradebot/Trader.h>

#include <spdlog/spdlog.h>

#include <boost/asio/post.hpp>


namespace ad {

struct OrderTracker
{
    tradebot::Order order;;
    tradebot::Fulfillment fulfillment;
};

struct NaiveBot
{
    tradebot::Trader trader;
    std::optional<OrderTracker> currentOrder;
    double percentage;
    boost::asio::io_context mainLoop;

    void onPartialFill(Json aReport);

    void onCompletion(Json aReport);

    void run();
};


inline void NaiveBot::onPartialFill(Json aReport)
{
    currentOrder->fulfillment.accumulate(
            tradebot::Fulfillment::fromStreamJson(aReport),
            currentOrder->order);
}


tradebot::Order & transformOrder(tradebot::Order & aOrder, double aPercentage)
{
    aOrder.reverseSide();
    if (aOrder.side == tradebot::Side::Buy)
    {
        aOrder.fragmentsRate = (1.0 - aPercentage) * aOrder.executionRate;
    }
    else
    {
        aOrder.fragmentsRate = (1.0 + aPercentage) * aOrder.executionRate;
    }
    return aOrder;
}


inline void NaiveBot::onCompletion(Json aReport)
{
    // Complete the order
    onPartialFill(aReport);
    // TODO sanity check regarding the report amount vs order amount vs fulfill amount?
    if(trader.completeOrder(currentOrder->order, currentOrder->fulfillment))
    {
        // Place the next order
        currentOrder.emplace(OrderTracker{
                tradebot::Order{transformOrder(currentOrder->order, percentage)},
                tradebot::Fulfillment{}
        });
        trader.placeNewOrder(tradebot::Execution::Limit, currentOrder->order);
    }
};


inline void NaiveBot::run()
{
    // Initial cleanup
    trader.cleanup();

    auto onReport = [this](Json aExecutionReport)
    {
        if (aExecutionReport.at("X") == "PARTIALLY_FILLED")
        {
            boost::asio::post(mainLoop, std::bind(&NaiveBot::onPartialFill, this, aExecutionReport));
        }
        else if (aExecutionReport.at("X") == "FILLED")
        {
            boost::asio::post(mainLoop, std::bind(&NaiveBot::onCompletion, this, aExecutionReport));
        }
        else if (aExecutionReport.at("X") == "NEW")
        {
            spdlog::info("Exchange accepted new order '{}'.", aExecutionReport.at("c"));
        }
    };

    trader.exchange.openUserStream(onReport);
    trader.placeNewOrder(tradebot::Execution::Market, currentOrder->order);

    mainLoop.run();
};


} // namespace ad
