#include "ProductionBot.h"

#include <cstdlib>


namespace ad {
namespace trade {


int ProductionBot::run()
{
    trader.cleanup();

    return EXIT_SUCCESS;
}


} // namespace trade
} // namespace ad
