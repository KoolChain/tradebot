#pragma once


#include "Fulfillment.h"
#include "Order.h"

#include <binance/Api.h>
#include <websocket/WebSocket.h>

#include <boost/asio/steady_timer.hpp>

#include <thread>


namespace ad {
namespace tradebot {


enum class Execution
{
    Market,
    Limit,
};

struct Exchange
{
    using UserStreamCallback = std::function<void(Json)>;

    class Stream
    {
    private:
        friend class Exchange;

        enum Status
        {
            Initialize,
            Connected,
            Done,
        };

    public:
        // Made public so std::optional::emplace can access it
        Stream(binance::Api & aRestApi, Exchange::UserStreamCallback aOnExecutionReport);
        ~Stream();

    private:
        Stream(const Stream &) = delete;
        Stream(Stream &&) = delete;
        Stream & operator = (const Stream &) = delete;
        Stream & operator = (Stream &&) = delete;


        void onListenKeyTimer(const boost::system::error_code & aErrorCode);

        // Synchronization mechanism for status variable (allowing to wait for connection)
        std::mutex mutex;
        std::condition_variable statusCondition;
        Status status{Initialize};

        // This class will compose Exchange, which is not movable nor copyable anyway
        // so we refer back into the parent instance.
        binance::Api & restApi;
        std::string listenKey;
        net::WebSocket websocket;
        boost::asio::steady_timer keepListenKeyAlive;
        bool intendedClose{false};
        std::thread thread;
    };

    /// \return "NOTEXISTING" if the exchange engine does not know the order, otherwise returns
    ///         the value from the exchange:
    ///         * NEW (Active order)
    ///         * CANCELED
    ///         * FILLED
    ///         * EXPIRED
    std::string getOrderStatus(const Order & aOrder);

    Decimal getCurrentAveragePrice(const Pair & aPair);

    Order placeOrder(Order & aOrder, Execution aExecution);

    std::optional<FulfilledOrder> fillMarketOrder(Order & aOrder);

    /// \return true if the order was cancelled, false otherwise
    ///         (because it was not present, already cancelled, etc.).
    bool cancelOrder(const Order & aOrder);

    std::vector<binance::ClientId> cancelAllOpenOrders(const Pair & aPair);

    std::vector<binance::ClientId> listOpenOrders(const Pair & aPair);

    Json queryOrder(const Order & aOrder);

    /// \param aPageSize The number of trades fetched with each API request.
    ///        Mainly usefull for writing tests.
    Fulfillment accumulateTradesFor(const Order & aOrder, int aPageSize=1000);

    /// \brief Blocks while opening a websocket to get spot user data stream.
    /// \return `true` if the websocket connected successfully, `false` otherwise.
    bool openUserStream(UserStreamCallback aOnExecutionReport);
    void closeUserStream();

    binance::Api restApi;
    std::optional<Stream> spotUserStream;
};


} // namespace tradebot
} // namespace ad
