#include "NaiveBot.h"

#include <binance/Api.h>

#include <tradebot/Exchange.h>
#include <tradebot/Trader.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fstream>
#include <iostream>

#include <cstdlib>


using namespace ad;


void configureLogging()
{
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/tmp/naivebot.log", true);
    fileSink->set_level(spdlog::level::trace);

    auto logger = std::make_shared<spdlog::logger>(spdlog::logger{"multi_sink", {consoleSink, fileSink}});
    logger->set_level(spdlog::level::trace);

    spdlog::set_default_logger(logger);
}


int main(int argc, char * argv[])
{
    // Hardcoded to testnet ATM
    if (argc != 6)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile base quote amount percentage\n";
        return EXIT_FAILURE;
    }

    configureLogging();

    std::string secretsfile{argv[1]};
    tradebot::Pair pair{argv[2], argv[3]};
    Decimal amount{std::stod(argv[4])};
    double percentage{std::stod(argv[5])};

    if (percentage < 0. || percentage > 1.)
    {
        throw std::domain_error{"Percentage must be in [0.0, 1.0]."};
    }

    const std::string databasePath{"/tmp/naivebot.sqlite"};

    spdlog::info("Starting {} to trade orders of {} {} with variations of {}%.",
            argv[0],
            amount,
            pair.base,
            percentage*100);

    NaiveBot bot{
        tradebot::Trader{
            "naivebot",
            pair,
            tradebot::Database{databasePath},
            tradebot::Exchange{
                binance::Api{std::ifstream{secretsfile}, binance::Api::gTestNet},
            },
        },
        OrderTracker{
            tradebot::Order{
                bot.trader.name,
                pair.base,
                pair.quote,
                amount,
                0.,
                tradebot::Side::Sell,
            },
            {}
        },
        percentage,
    };

    bot.run();

    return EXIT_SUCCESS;
}
