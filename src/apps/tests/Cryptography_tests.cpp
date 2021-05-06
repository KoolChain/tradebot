#include "catch.hpp"

#include <binance/Cryptography.h>


using namespace ad;


SCENARIO("Hashing function")
{
    //see: https://binance-docs.github.io/apidocs/spot/en/#signed-trade-user_data-and-margin-endpoint-security
    GIVEN("A sample message and key from binance API doc")
    {
        const std::string message{"symbol=LTCBTC&side=BUY&type=LIMIT&timeInForce=GTC&quantity=1&price=0.1&recvWindow=5000&timestamp=1499827319559"};
        const std::string key{"NhqPtmdSJYdKjVHjA7PZj4Mge3R5YNiP1e3UZjInClVN65XAbvqqM6A7H5fATj0j"};
        THEN("It produces expected result when hashed with SHA256")
        {
            const std::string expected{"c8db56825ae71d6d79447849e617115f4a920fa2acdcab2b053c4b2838bd6b71"};
            REQUIRE(crypto::encodeHexadecimal(crypto::hashMacSha256(key, message)) == expected);
        }
    }
}
