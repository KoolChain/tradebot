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

    auto systemStatus = binanceapi::Api::getSystemStatus();
    if (systemStatus.json)
    {
        std::cout
            << *(systemStatus.json) << '\n'
            << std::endl;
    }

    std::ofstream dumpfile{argv[2]};
    dumpfile << binanceapi::Api::getExchangeInformation().json->dump(4)
             << "\n\n---------\n\n";


    binanceapi::Api binance{std::ifstream{argv[1]}};
    dumpfile << binance.getAccountInformation().json->dump(4)
             << "\n\n---------\n\n";

    return EXIT_SUCCESS;
}
