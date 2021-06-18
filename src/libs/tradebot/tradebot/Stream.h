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


using StreamCallback = std::function<void(Json)>;

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
    Stream(binance::Api & aRestApi, StreamCallback aOnExecutionReport);
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
    boost::asio::io_context listenKeyIoContext;
    boost::asio::steady_timer keepListenKeyAlive;
    std::thread listenKeyThread;
    net::WebSocket websocket;
    std::atomic<bool> intendedClose{false}; // accessed from both timer and websocket threads
    std::thread websocketThread;
};


} // namespace tradebot
} // namespace ad
