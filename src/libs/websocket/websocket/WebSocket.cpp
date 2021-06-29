#include "WebSocket.h"

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>

#include <spdlog/spdlog.h>


namespace net = boost::asio;
namespace beast = boost::beast;


#if 0
// Read/Write
// TODO ??
//ws.text(true);

// Note: pong response should be done automatically by Beast
// see: https://www.boost.org/doc/libs/1_76_0/libs/beast/doc/html/beast/using_websocket/control_frames.html

// Optional control callback
ws.control_callback(
    [](frame_type kind, string_view payload)
    {
        // Do something with the payload
        boost::ignore_unused(kind, payload);
    });

//
// Enable idle_timeout?
//
stream_base::timeout opt{
    std::chrono::seconds(30),   // handshake timeout
    stream_base::none(),        // idle timeout
    false
};

// Set the timeout options on the stream.
ws.set_option(opt);
#endif

namespace ad {
namespace net {


void logFailure(beast::error_code aErrorCode, const std::string & aWhat)
{
    // The outstanding read (and potential write) will be aborted
    // on close. This is normal.
    if (! (aErrorCode == ::net::error::operation_aborted
           // Present in the "Chat Server" Beast example
           //|| aErrorCode == beast::websocket::error::closed
           // To shut up "stream truncated" error
           // see: https://github.com/boostorg/beast/issues/824
           //|| aErrorCode == ::net::ssl::error::stream_truncated
          ))
    {
        spdlog::error("{} error: {}", aWhat, aErrorCode.message());
    }
    else
    {
        spdlog::trace("{} operation aborted: {}.", aWhat, aErrorCode.message());
    }
}


struct WebSocket::impl
{
    void run(std::string aHost, const std::string & aPort, const std::string & aTarget);

    void onHandshake(beast::error_code aErrorCode);
    void onWrite(beast::error_code aErrorCode, std::size_t aBytesTransferred);
    void onRead(beast::error_code aErrorCode, std::size_t aBytesTransferred);
    void onClose(beast::error_code aErrorCode);

    void writeImplementation();
    void writeNext();

    void readNext();

    void async_send(const std::string & aMessage);
    void async_close();

    static const std::string gFifoGuard;

    ::net::io_context mIoc;
    ::net::ssl::context mSslCtx{::net::ssl::context::tlsv12_client};
    beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>> mStream{mIoc, mSslCtx};
    beast::flat_buffer mBuffer;

    std::mutex mWriteMutex;
    // The initial value prevents call to async_send before connection is established
    // from trying to send immediatly.
    std::queue<std::string> mMessageFifo{{gFifoGuard}};

    std::atomic<bool> mClosing{false};

    WebSocket::ConnectCallback mConnectCallback{[](){}};
    WebSocket::ReceiveCallback mReceiveCallback{[](const std::string &){}};
};


const std::string WebSocket::impl::gFifoGuard{"NOT-CONNECTED-GUARD"};


void WebSocket::impl::run(std::string aHost, const std::string & aPort, const std::string & aTarget)
{
    ::net::ip::tcp::resolver resolver{mIoc};

    // Connect the socket to the IP address returned from performing a name lookup
    ::net::ip::tcp::endpoint endpoint =
        get_lowest_layer(mStream).connect(resolver.resolve(aHost, aPort));
    spdlog::debug("Connected to '{}' on port {}.", aHost, aPort);

    // Update the host string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    aHost += ':' + std::to_string(endpoint.port());

    // Set a timeout on the operation
    beast::get_lowest_layer(mStream).expires_after(std::chrono::seconds(20));

    // Set SNI Hostname (many hosts need this to handshake successfully)
    // Binance does require it
    if(! SSL_set_tlsext_host_name(
            mStream.next_layer().native_handle(),
            aHost.c_str()))
    {
        auto ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                                    ::net::error::get_ssl_category());
        return logFailure(ec, "Connect");
    }

    // SSL handshake
    mStream.next_layer().handshake(::net::ssl::stream_base::client);
    spdlog::trace("SSL handshake complete.");

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(mStream).expires_never();

    // Set suggested timeout settings for the websocket
    mStream.set_option(beast::websocket::stream_base::timeout::suggested(beast::role_type::client));

    // Do the websocket handshake in the client role, on the connected stream.
    // The implementation only uses the Host parameter to set the HTTP "Host" field,
    // it does not perform any DNS lookup. That must be done first, as shown above.
    mStream.async_handshake(
        aHost,  // The Host field
        aTarget,     // The request-target
        std::bind(&impl::onHandshake, this, std::placeholders::_1)
    );
}


void WebSocket::impl::onHandshake(beast::error_code aErrorCode)
{
    if(aErrorCode)
    {
        logFailure(aErrorCode, "Websocket handshake");
    }
    else
    {
        spdlog::trace("Websocket handshake complete.");

        spdlog::info("Websocket connection established.");

        // Start reading incoming messages
        readNext();

        mConnectCallback();

        // Pop the initial guard from the message queue
        // then tries to send if other were queued by client
        writeNext();
    }
}


void WebSocket::impl::onClose(beast::error_code aErrorCode)
{
    if(aErrorCode)
    {
        logFailure(aErrorCode, "Websocket close");
    }
    else
    {
        spdlog::info("Websocket closed.");
    }

    // Clear the fifo and reset the guard for next connection.
    mMessageFifo = decltype(mMessageFifo){{gFifoGuard}};
    mClosing = false;
}


void WebSocket::impl::onRead(beast::error_code aErrorCode, std::size_t aBytesTransferred)
{
    if(aErrorCode)
    {
        logFailure(aErrorCode, "Websocket read");
    }
    else
    {
        // Disabled: tends to represent the vast majority of logging volume of trading bots
        //spdlog::trace("Successfully read {} bytes: {}.",
        //              aBytesTransferred,
        //              beast::buffers_to_string(mBuffer.cdata()));

        mReceiveCallback(beast::buffers_to_string(mBuffer.cdata()));
        mBuffer.consume(aBytesTransferred);
        readNext();
    }
}


void WebSocket::impl::readNext()
{
    mStream.async_read(
        mBuffer,
        std::bind(&impl::onRead, this, std::placeholders::_1, std::placeholders::_2));
}


void WebSocket::impl::onWrite(beast::error_code aErrorCode, std::size_t aBytesTransferred)
{
    if(aErrorCode)
    {
        logFailure(aErrorCode, "Websocket write");
    }
    else
    {
        spdlog::trace("Successfully wrote {} bytes.", aBytesTransferred);
        writeNext();
    }
}


void WebSocket::impl::writeNext()
{
    std::scoped_lock queueLock{mWriteMutex};
    mMessageFifo.pop(); // pop the message corresponding to this completion handler
    if (! mMessageFifo.empty())
    {
        writeImplementation();
    }
}


void WebSocket::impl::async_send(const std::string & aMessage)
{
    {
        std::scoped_lock queueLock{mWriteMutex};
        mMessageFifo.push(aMessage);
        // If the queue was empty, there is no async_write already going on
        if (mMessageFifo.size() > 1)
        {
            return;
        }

        // Note: It might be possible to take this call out of the critical section
        // **if** it is safe to read the front of a std::queue while pushing concurrently.
        writeImplementation();
    }
}


void WebSocket::impl::async_close()
{
    if (! mClosing.exchange(true))
    {
        spdlog::trace("Request for websocket to close.");
        mStream.async_close(
            beast::websocket::close_code::normal,
            std::bind(&impl::onClose, this, std::placeholders::_1));
    }
    else
    {
        spdlog::trace("Already closing the websocket.");
    }
}


void WebSocket::impl::writeImplementation()
{
    // The message will be popped by the completion handler
    mStream.async_write(
            ::net::buffer(mMessageFifo.front()),
            std::bind(&impl::onWrite, this, std::placeholders::_1, std::placeholders::_2));
}


WebSocket::WebSocket() :
        mImpl{std::make_unique<impl>()}
{}


WebSocket::~WebSocket()
{}


void WebSocket::run(const std::string & aHost, const std::string & aPort, const std::string & aTarget)
{
    mImpl->run(aHost, aPort, aTarget);
    mImpl->mIoc.run();
    spdlog::trace("Websocket run completed.");
}


void WebSocket::async_send(const std::string & aMessage)
{
    mImpl->async_send(aMessage);
}


void WebSocket::async_close()
{
    mImpl->async_close();
}


void WebSocket::setConnectCallback(ConnectCallback aOnConnect)
{
    mImpl->mConnectCallback = std::move(aOnConnect);
}


void WebSocket::setReceiveCallback(ReceiveCallback aOnReceive)
{
    mImpl->mReceiveCallback = std::move(aOnReceive);
}


::net::io_context & WebSocket::exposeContextDetail()
{
    return mImpl->mIoc;
}


} // namespace net
} // namespace ad
