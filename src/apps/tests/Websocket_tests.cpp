#include "catch.hpp"

#include <websocket/WebSocket.h>

#include <thread>


SCENARIO("Websocket simple communication")
{
    GIVEN("An echo websocket server and a message")
    {
        const std::string host = "wss://echo.websocket.org";
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
            ws.run("echo.websocket.org", "443");
            REQUIRE(received == message);
        }
    }
}
