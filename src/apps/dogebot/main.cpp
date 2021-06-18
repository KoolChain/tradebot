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
    logger->set_pattern("[%t][%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");

    spdlog::set_default_logger(logger);
}


int runNaiveBot(int argc, char * argv[], const std::string & aSecretsFile)
{
    if (argc != 7)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile naivebot base quote amount percentage\n";
        return EXIT_FAILURE;
    }

    tradebot::Pair pair{argv[3], argv[4]};
    Decimal amount{argv[5]};
    double percentage{std::stod(argv[6])};

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
                binance::Api{std::ifstream{aSecretsFile}, binance::Api::gTestNet},
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


int main(int argc, char * argv[])
{
    // Hardcoded to testnet ATM
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile botname [bot-args...]\n";
        return EXIT_FAILURE;
    }

    configureLogging();
    std::string secretsfile{argv[1]};

    if (argv[2] == std::string{"naivebot"} || argv[2] == std::string{"NaiveBot"})
    {
        return runNaiveBot(argc, argv, secretsfile);
    }
    else
    {
        std::cerr << "Unknown botname '" << argv[2] << "'.\n";
        std::cerr << "Usage: " << argv[0] << " secretsfile botname [bot-args...]\n";
        return EXIT_FAILURE;
    }
}
