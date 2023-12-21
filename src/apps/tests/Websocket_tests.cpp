#include "catch.hpp"

#include <websocket/WebSocket.h>

#include <thread>


SCENARIO("Websocket simple communication", "[.]")
{
    GIVEN("An echo websocket server and a message")
    {
        // TODO Ad 2023/12/21: Find an echo websocket server that is still working.
        // or address the "sslv3 alert handshake failure" with ws.ifelse.io
        //const std::string host = "echo.websocket.org";
        const std::string host = "ws.ifelse.io";
        const std::string message = "I am DOGE";

        THEN("A connection can be established, and message repeated")
        {
            std::string received;
            ad::net::WebSocket ws{
                // on receive
                [&ws, &received](const std::string & aMessage)
                {
                    received = aMessage;
                    ws.async_close();
                }};
            ws.async_send(message);
            ws.run(host, "443");
            REQUIRE(received == message);
        }
    }
}
