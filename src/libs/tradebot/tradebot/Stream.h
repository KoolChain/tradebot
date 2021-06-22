#pragma once


#include <binance/Api.h>

#include <websocket/WebSocket.h>

#include <boost/asio/steady_timer.hpp>

#include <condition_variable>
#include <functional>
#include <thread>


namespace ad {
namespace tradebot {


class Exchange;


/// \brief Allows to run a user provided operation at regular interval.
///
/// Notably useful to PUT the listen key for Binance's User Data Stream.
class RefreshTimer
{
public:
    using MaintenanceOperation = std::function<void(void)>;
    using Duration = std::chrono::milliseconds;

    RefreshTimer(MaintenanceOperation aOperation, Duration aPeriod);
    ~RefreshTimer();

    void async_wait();

private:
    void onTimer(const boost::system::error_code & aErrorCode);

private:
    MaintenanceOperation operation;
    Duration period;

    bool intendedClose{false};
    boost::asio::io_context ioContext;
    boost::asio::steady_timer timer;
    std::thread thread;
};


/// \brief Convenient way to give a websocket destination as a single parameter.
struct WebsocketDestination
{
    std::string host;
    std::string port;
    std::string target;
};


/// \brief Abstraction over the notion of websocket stream as provided by Binance.
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
    using ReceiveCallback = std::function<void(Json)>;
    using UnintendedCloseCallback = std::function<void(void)>;

    // Made public so std::optional::emplace can access it
    Stream(WebsocketDestination aDestination,
           ReceiveCallback aOnMessage,
           UnintendedCloseCallback aOnUnintededClose,
           std::unique_ptr<RefreshTimer> aKeepAlive = nullptr);
    ~Stream();

private:
    Stream(const Stream &) = delete;
    Stream(Stream &&) = delete;
    Stream & operator = (const Stream &) = delete;
    Stream & operator = (Stream &&) = delete;

    // Synchronization mechanism for status variable (allowing to wait for connection)
    std::mutex mutex;
    std::condition_variable statusCondition;
    Status status{Initialize};

    // An optional refresh timer, if periodic refresh is needed by the connected stream.
    std::unique_ptr<RefreshTimer> keepAlive;

    net::WebSocket websocket;
    std::atomic<bool> intendedClose{false}; // accessed from both the thread destruction stream an the inner thread.
    std::thread websocketThread;
};


} // namespace tradebot
} // namespace ad
