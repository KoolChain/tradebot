#include "NaiveBot.h"
#include "ProductionBot.h"

#include <binance/Api.h>

#include <tradebot/Exchange.h>
#include <tradebot/spawners/StableDownSpread.h>
#include <tradebot/Trader.h>

#include <trademath/Spreaders.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fstream>
#include <iostream>

#include <cstdlib>


using namespace ad;


void configureLogging(const std::string aBotname, const std::string aLogPathPrefix = "./")
{
    //
    // Stdout sink
    //
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);

    auto makeLogpath = [&](const std::string & aSuffix)
    {
        std::ostringstream logpath;
        logpath << aLogPathPrefix << aBotname << "-" << aSuffix
              /*<< "-" << getTimestamp() */ // Append to the same file for log rotation
                << ".log";
        return logpath.str();
    };

    //
    // Rotating log file showing everything
    //
    auto maxSize = 10 * (1024*1024)/*Mb*/;
    auto maxFiles = 3;
    auto rotatingFileSink =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(makeLogpath("rotating"),
                                                               maxSize,
                                                               maxFiles);
    rotatingFileSink->set_level(spdlog::level::trace);
    rotatingFileSink->set_pattern("[%t][%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");

    //
    // Permanent log file, showing only warnings and above
    //
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(makeLogpath("warnings"), false/*do not truncate*/);
    fileSink->set_level(spdlog::level::warn);
    fileSink->set_pattern("[%t][%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");

    auto logger = std::make_shared<spdlog::logger>(spdlog::logger{"multi_sink",
                                                                  {
                                                                    consoleSink,
                                                                    rotatingFileSink,
                                                                    fileSink
                                                                  }});
    // Otherwise, the traces are not written to the sinks, even when they set it explicitly
    logger->set_level(spdlog::level::trace);

    // Now, only set the custom pattern on file sink (as it disables the color output on stdout)
    //logger->set_pattern("[%t][%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");

    // On unhandled exception exit,
    // it seems the file logs do not show everything (even though there is a catch all).
    // Try to flush starting at warning level.
    logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(logger);
}


int runNaiveBot(int argc, char * argv[], const std::string & aSecretsFile, const Json & /*aConfig*/)
{
    if (argc != 8)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile config-path naivebot base quote amount percentage\n";
        return EXIT_FAILURE;
    }

    tradebot::Pair pair{argv[4], argv[5]};
    Decimal amount{argv[6]};
    double percentage{std::stod(argv[7])};

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


int runProductionBot(int argc, char * argv[], const std::string & aSecretsFile, const Json & aConfig)
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile config-path productionbot database-path\n";
        return EXIT_FAILURE;

    }

    const std::string databasePath{argv[4]};

    tradebot::Pair pair{aConfig.at("base"), aConfig.at("quote")};
    //Decimal amount{aConfig.at("amount")};
    Decimal firstStop{aConfig.at("ladder").at("firstStop").get<std::string>()};
    Decimal factor{aConfig.at("ladder").at("factor").get<std::string>()};
    int stopCount = std::stoi(aConfig.at("ladder").at("stopCount").get<std::string>());
    Decimal exchangeTickSize{aConfig.at("ladder").at("exchangeTickSize").get<std::string>()};
    Decimal internalTickSize{aConfig.at("ladder").at("internalTickSize").get<std::string>()};
    Decimal priceOffset{aConfig.at("ladder").at("priceOffset").get<std::string>()};
    const std::string botName =
        aConfig.at("bot").value("name", "productionbot") + '_' + std::to_string(getTimestamp());

    Json spawnerConfig = aConfig.at("spawner");

    spdlog::info("Starting bot '{}' to trade {}.",
                 botName,
                 pair.symbol());

    //
    // Ladder
    //
    trade::Ladder ladder = trade::makeLadder(firstStop,
                                             factor,
                                             stopCount,
                                             exchangeTickSize,
                                             internalTickSize,
                                             priceOffset);

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
    // Copy the values read from the spawnerConfig["spreader"]["proporitions"] in `proportions`.
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
        std::cerr << "Usage: " << argv[0] << " secretsfile config-path botname [bot-args...]\n";
        return EXIT_FAILURE;
    }

    const std::string secretsfile{argv[1]};

    Json config = Json::parse(std::ifstream{argv[2]});
    const std::string logPrefix = config.value("logPath", "./");

    const std::string botname{argv[3]};

    configureLogging(botname, logPrefix);


    // The topmost catch all, to ensure stack unwinding (notably flushing logs).
    try
    {
        if (botname == std::string{"naivebot"} || botname == std::string{"NaiveBot"})
        {
            return runNaiveBot(argc, argv, secretsfile, config);
        }
        else if (botname == std::string{"productionbot"} || botname == std::string{"productionbot"})
        {
            return runProductionBot(argc, argv, secretsfile, config);
        }
        else
        {
            std::cerr << "Unknown botname '" << botname << "'.\n";
            std::cerr << "Usage: " << argv[0] << " secretsfile config-path botname [bot-args...]\n";
            return EXIT_FAILURE;
        }
    }
    catch (std::exception & aException)
    {
        spdlog::critical("Unhandled exception while running '{}': {}.",
                botname,
                aException.what());
        return EXIT_FAILURE;
    }
}
