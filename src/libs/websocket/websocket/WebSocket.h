#pragma once


#include <experimental/propagate_const>

#include <mutex>
#include <queue>


namespace ad {
namespace net {


class WebSocket
{
    using ReceiveCallback = std::function<void(const std::string &)>;

public:
    WebSocket();

    /// \brief Constructor accepting a receive callback.
    /// \param aOnReceive Callback invoked when a message is received.
    /// \important All handlers are running in a single thread (implicit strand),
    /// the thread in which `run()` has been called.
    template <class T_receiveHandler>
    WebSocket(T_receiveHandler && aOnReceive);

    ~WebSocket();

    /// \brief Connects the websocket, which will start sending and receiving messages.
    /// \attention Blocks the calling thread until the websocket is closed.
    void run(const std::string & aHost, const std::string & aPort, const std::string & aTarget = "/");

    /// \brief Queue a message to be sent at first opportunity, thread safe.
    /// \note Can be called while the WebSocket is running, or not running.
    void async_send(const std::string & aMessage);

    /// \brief Queue a message.
    /// \attention **Not** thread safe.
    void async_close();

private:
    void setReceiveCallback(ReceiveCallback aOnReceive);

private:
    struct impl;
    std::experimental::propagate_const<std::unique_ptr<impl>> mImpl;
};


template <class T_receiveHandler>
WebSocket::WebSocket(T_receiveHandler && aOnReceive) :
    WebSocket{}
{
    setReceiveCallback(std::forward<T_receiveHandler>(aOnReceive));
}


} // namespace net
} // namespace ad
