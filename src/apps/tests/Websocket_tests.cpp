#include "catch.hpp"

#include <websocket/WebSocket.h>


SCENARIO("Websocket communication")
{
    GIVEN("A websocket server")
    {
        const std::string host = "wss://echo.websocket.org";
        THEN("A connection can be established")
        {
            ad::net::WebSocket ws{};
            ws.run("echo.websocket.org", "443");
            REQUIRE(true);
        }
    }
}
