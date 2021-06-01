#include "Fulfillment.h"

#include "Order.h"

#include <spdlog/spdlog.h>


namespace ad {
namespace tradebot {


Fulfillment & Fulfillment::accumulate(const Fulfillment & aRhs, const Order & aContext)
{
    amountBase += aRhs.amountBase;
    amountQuote += aRhs.amountQuote;
    fee += aRhs.fee;

    if (feeAsset == "")
    {
        feeAsset = aRhs.feeAsset;
    }
    else if (feeAsset != aRhs.feeAsset)
    {
        spdlog::critical("Inconsistent commissions assets {} vs. {} in fills for order '{}'.",
                         feeAsset,
                         aRhs.feeAsset,
                         static_cast<const std::string &>(aContext.clientId())
                         );
        throw std::logic_error{"Different fills for the same order have different commissions assets."};
    }

    latestTrade = std::max(latestTrade, aRhs.latestTrade);
    tradeCount += aRhs.tradeCount;
    return *this;
}


} // namespace tradebot
} // namespace ad
