#pragma once


#include <binance/Time.h>

#include <trademath/Decimal.h>


namespace ad {
namespace tradebot {
namespace stats {


struct Balance
{
    MillisecondsSinceEpoch time;

    Decimal baseBalance;
    Decimal quoteBalance;

    // Base for which buy fragments exist (base increase)
    Decimal baseBuyPotential;
    // Quote cost corresponding to buying all base above (quote decrease)
    Decimal quoteBuyPotential;

    // Base for which sell fragments exist (base decrease)
    Decimal baseSellPotential;
    // Quote total corresponding to selling all base above (quote increase)
    Decimal quoteSellPotential;

    long id{-1}; // auto-increment by ORM
};


} // namespace stats
} // namespace tradebot
} // namespace ad
