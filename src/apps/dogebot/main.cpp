#include <binance/Api.h>

#include <fstream>
#include <iostream>

#include <cstdlib>

#include <spdlog/spdlog.h>


using namespace ad;

int main(int argc, char * argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << "secretsfile dumpfile\n";
        return EXIT_FAILURE;
    }

    spdlog::set_level(spdlog::level::trace);

    binance::Api binance{std::ifstream{argv[1]}, binance::Api::gProduction};

    std::ofstream dumpfile{argv[2]};

    //auto systemStatus = binance::Api::getSystemStatus();
    //if (systemStatus.json)
    //{
    //    std::cout
    //        << *(systemStatus.json) << '\n'
    //        << std::endl;
    //}
#if 0
    {
        dumpfile << binance.getExchangeInformation().json->dump(3)
                 << "\n\n---------\n\n";
    }


    {
        dumpfile << binance.getAccountInformation().json->dump(4)
                 << "\n\n---------\n\n";
    }
#endif

    {
        std::string listenKey = binance.createSpotListenKey().json->at("listenKey");
        std::cerr << "Listenkey:" << listenKey << '\n';

        net::WebSocket stream{};
        stream.run(binance::Api::gProduction.websocketHost,
                   binance::Api::gProduction.websocketPort,
                   "/ws/"+listenKey);
    }
    return EXIT_SUCCESS;
}
