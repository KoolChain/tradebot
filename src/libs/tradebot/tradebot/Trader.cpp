#include "Trader.h"

namespace ad {
namespace tradebot {


int Trader::cancelOpenOrders()
{
    binance::Response response = binance.restApi.cancelAllOpenOrders(pair.symbol());

    int count{0};
    if (response.status != 200)
    {

    }

    return count;
}


} // namespace tradebot
} // namespace ad
