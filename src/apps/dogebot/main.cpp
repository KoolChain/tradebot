#include "NaiveBot.h"
#include "ProductionBot.h"

#include <binance/Api.h>

#include <tradebot/Exchange.h>
#include <tradebot/spawners/StableDownSpread.h>
#include <tradebot/Trader.h>

#include <trademath/Spreaders.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fstream>
#include <iostream>

#include <cstdlib>


using namespace ad;


void configureLogging(const std::string aBotname, const std::string aLogPathPrefix = "./")
{
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);

    std::ostringstream logpath;
    logpath << aLogPathPrefix << aBotname << "-" << getTimestamp() << ".log";
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logpath.str(), true);
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
                binance::Api{std::ifstream{aSecretsFile}},
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


int runProductionBot(int argc, char * argv[], const std::string & aSecretsFile)
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile productionbot database-path config-path\n";
        return EXIT_FAILURE;

    }

    const std::string databasePath{argv[3]};

    Json config = Json::parse(std::ifstream{argv[4]});
    tradebot::Pair pair{config.at("base"), config.at("quote")};
    //Decimal amount{config.at("amount")};
    Decimal firstStop{config.at("ladder").at("firstStop").get<std::string>()};
    Decimal factor{config.at("ladder").at("factor").get<std::string>()};
    int stopCount = std::stoi(config.at("ladder").at("stopCount").get<std::string>());
    Decimal tickSize{config.at("ladder").at("tickSize").get<std::string>()};
    const std::string botName =
        config.at("bot").value("name", "productionbot") + '_' + std::to_string(getTimestamp());

    Json spawnerConfig = config.at("spawner");

    spdlog::info("Starting bot '{}' to trade {}.",
                 botName,
                 pair.symbol());

    //
    // Ladder
    //
    trade::Ladder ladder = trade::makeLadder(firstStop, factor, stopCount, tickSize);

    //
    // Production Bot
    //
    trade::ProductionBot bot{
        tradebot::Trader{
            botName,
            pair,
            tradebot::Database{databasePath},
            tradebot::Exchange{
                binance::Api{std::ifstream{aSecretsFile}},
            },
        },
        trade::IntervalTracker{
            ladder
        },
    };

    //
    // spawner::StableDownSpread
    //
    std::vector<Decimal> proportions;
    std::transform(spawnerConfig.at("spreader").at("proportions").begin(),
                   spawnerConfig.at("spreader").at("proportions").end(),
                   std::back_inserter(proportions),
                   [](const std::string & aValue){ return Decimal{aValue}; });

    bot.trader.spawner =
        std::make_unique<tradebot::spawner::StableDownSpread<trade::ProportionSpreader>>(
            trade::ProportionSpreader{
                ladder,
                proportions,
                bot.trader.queryFilters().amount.tickSize,
            },
            Decimal{spawnerConfig.at("takeHomeFactorInitialSell").get<std::string>()},
            Decimal{spawnerConfig.at("takeHomeFactorSubsequentSell").get<std::string>()},
            Decimal{spawnerConfig.at("takeHomeFactorSubsequentBuy").get<std::string>()}
        );

    //
    // Run
    //
    bot.trader.recordLaunch();
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

    const std::string botname{argv[2]};
    configureLogging(botname);
    const std::string secretsfile{argv[1]};

    if (botname == std::string{"naivebot"} || botname == std::string{"NaiveBot"})
    {
        return runNaiveBot(argc, argv, secretsfile);
    }
    else if (botname == std::string{"productionbot"} || botname == std::string{"productionbot"})
    {
        return runProductionBot(argc, argv, secretsfile);
    }
    else
    {
        std::cerr << "Unknown botname '" << botname << "'.\n";
        std::cerr << "Usage: " << argv[0] << " secretsfile botname [bot-args...]\n";
        return EXIT_FAILURE;
    }
}
