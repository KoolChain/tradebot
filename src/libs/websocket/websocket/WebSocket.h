#pragma once



#if not defined(_MSC_VER)
#include <experimental/propagate_const>
#endif

#include <functional>
#include <memory>
#include <mutex>
#include <queue>


// Some nasty forward declaration to support nasty violations of encapsulation.
namespace boost {
namespace asio {

class io_context;

} // namespace asio
} // namespace boost


namespace ad {
namespace net {


class WebSocket
{
    using ConnectCallback = std::function<void()>;
    using ReceiveCallback = std::function<void(const std::string &)>;

public:
    WebSocket();

    /// \brief Constructor accepting a receive callback.
    /// \param aOnReceive Callback invoked when a message is received.
    /// \important All handlers are running in a single thread (implicit strand),
    /// the thread in which `run()` has been called.
    template <class T_receiveHandler>
    explicit WebSocket(T_receiveHandler && aOnReceive);

    /// \brief Constructor accepting a connect and a receive callback.
    ///
    /// \param aOnConnect Callback invoked when the connection is established.
    /// It will be invoked before any receive callback
    /// and before the first message is sent.
    /// But it is not guaranteed to be able to send the first message
    /// (if previous messages were queued with async_send before invoking `run()`).
    ///
    /// \param aOnReceive Callback invoked when a message is received.
    template <class T_connectHandler, class T_receiveHandler>
    WebSocket(T_connectHandler && aOnConnect, T_receiveHandler && aOnReceive);

    ~WebSocket();

    /// \brief Connects the websocket, which will start sending and receiving messages.
    /// \attention Blocks the calling thread until the websocket is closed.
    void run(const std::string & aHost, const std::string & aPort, const std::string & aTarget = "/");

    /// \brief Queue a message to be sent at first opportunity, thread safe.
    /// \note Can be called while the WebSocket is running, or not running.
    void async_send(const std::string & aMessage);

    /// \brief Queue a message, thread safe.
    void async_close();

    /// \brief Return the associated ASIO io_context.
    ///
    /// \attention This method violates encapsulation, but is a convenience
    /// allowing to add other asynchronous operations, such as timers.
    boost::asio::io_context & exposeContextDetail();

private:
    void setConnectCallback(ConnectCallback aOnConnect);
    void setReceiveCallback(ReceiveCallback aOnReceive);

private:
    struct Impl;
#if not defined(_MSC_VER)
    std::experimental::propagate_const<std::unique_ptr<Impl>> mImpl;
#else
    std::unique_ptr<Impl> mImpl;
#endif
};


template <class T_receiveHandler>
WebSocket::WebSocket(T_receiveHandler && aOnReceive) :
    WebSocket{}
{
    setReceiveCallback(std::forward<T_receiveHandler>(aOnReceive));
}


template <class T_connectHandler, class T_receiveHandler>
WebSocket::WebSocket(T_connectHandler && aOnConnect, T_receiveHandler && aOnReceive) :
    WebSocket{}
{
    setConnectCallback(std::forward<T_connectHandler>(aOnConnect));
    setReceiveCallback(std::forward<T_receiveHandler>(aOnReceive));
}


} // namespace net
} // namespace ad
