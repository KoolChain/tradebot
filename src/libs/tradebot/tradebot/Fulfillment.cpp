#include "Fulfillment.h"

#include "Logging.h"
#include "Order.h"


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
                         aContext.getIdentity()
                         );
        throw std::logic_error{"Different fills for the same order have different commissions assets."};
    }

    latestTrade = std::max(latestTrade, aRhs.latestTrade);
    tradeCount += aRhs.tradeCount;
    return *this;
}


std::ostream & operator<<(std::ostream & aOut, const Fulfillment & aFulfillment)
{
    return aOut << "Fulfillment over " << aFulfillment.tradeCount << " trade(s),"
                << " for " << aFulfillment.amountBase << " @ " << aFulfillment.amountQuote
                << " (price: " << aFulfillment.price() << ")."
                << " Fee: " << aFulfillment.fee << " " << aFulfillment.feeAsset << ".";
}

} // namespace tradebot
} // namespace ad
