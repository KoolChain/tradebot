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


struct WebSocket::impl
{
    ::net::io_context mIoc;
    ::net::ssl::context mSslCtx{::net::ssl::context::tlsv12_client};
    beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>> mStream{mIoc, mSslCtx};
    beast::flat_buffer mBuffer;

    void run(std::string aHost, const std::string & aPort);

    void onWrite(beast::error_code aErrorCode, std::size_t aBytesTransferred);
    void onRead(beast::error_code aErrorCode, std::size_t aBytesTransferred);
    void onClose(beast::error_code aErrorCode);
};


void WebSocket::impl::run(std::string aHost, const std::string & aPort)
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

    // TODO ??
    //// Set SNI Hostname (many hosts need this to handshake successfully)
    //if(! SSL_set_tlsext_host_name(
    //        ws_.next_layer().native_handle(),
    //        host_.c_str()))
    //{
    //    ec = beast::error_code(static_cast<int>(::ERR_get_error()),
    //        net::error::get_ssl_category());
    //    return fail(ec, "connect");
    //}

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
    mStream.handshake(
        aHost,  // The Host field
        "/"     // The request-target
    );
    spdlog::trace("Websocket handshake complete.");

    // TODO implement some usable API instead of hardcoded test
    mStream.async_write(
        ::net::buffer("I am the DOGE"),
        std::bind(&impl::onWrite, this, std::placeholders::_1, std::placeholders::_2));

    // Read a message into our buffer
    mStream.async_read(
        mBuffer,
        std::bind(&impl::onRead, this, std::placeholders::_1, std::placeholders::_2));

    spdlog::info("Websocket connection established.");
}


void logFailure(beast::error_code aErrorCode, const std::string & aWhat)
{
    spdlog::error("{}: {}", aWhat, aErrorCode.message());
}


void WebSocket::impl::onWrite(beast::error_code aErrorCode, std::size_t aBytesTransferred)
{
    if(aErrorCode)
    {
        logFailure(aErrorCode, "Websocket write error");
    }
    else
    {
        spdlog::debug("Successfully wrote {} bytes.", aBytesTransferred);
    }
}


void WebSocket::impl::onClose(beast::error_code aErrorCode)
{
    if(aErrorCode)
    {
        logFailure(aErrorCode, "Websocket close error");
    }
    else
    {
        spdlog::info("Websocket closed.");
    }
}


void WebSocket::impl::onRead(beast::error_code aErrorCode, std::size_t aBytesTransferred)
{
    if(aErrorCode)
    {
        logFailure(aErrorCode, "Websocket read error");
    }
    else
    {
        spdlog::debug("Successfully read {} bytes: {}.",
                      aBytesTransferred,
                      beast::buffers_to_string(mBuffer.cdata()));
        mStream.async_close(
            beast::websocket::close_code::normal,
            std::bind(&impl::onClose, this, std::placeholders::_1));

    }
}


WebSocket::WebSocket() :
        mImpl{std::make_unique<impl>()}
{}


WebSocket::~WebSocket()
{}


void WebSocket::run(const std::string & aHost, const std::string & aPort)
{
    mImpl->run(aHost, aPort);
    mImpl->mIoc.run();
}


} // namespace net
} // namespace ad
