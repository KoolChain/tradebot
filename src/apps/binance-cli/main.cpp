#include <binance/Api.h>

#include <tradebot/Database.h>
#include <tradebot/Exchange.h>
#include <tradebot/Logging.h>
#include <tradebot/Order.h>
#include <tradebot/Trader.h>


#include <boost/lexical_cast.hpp>

#include <fstream>
#include <iostream>
#include <sstream>


using namespace ad;


tradebot::Exchange getExchange(const std::string & aSecretsFile)
{
    return tradebot::Exchange{
        binance::Api{std::ifstream{aSecretsFile}}
    };
}


void printUsage(const std::string aCommand)
{
    std::cerr << "Usage: " << aCommand << " secretsfile action args... \n"
        << "\taction might be: buy, sell, query-order, exchange-info, account-info, filters, average-price\n"
        ;
}

int main(int argc, char * argv[])
{
    spdlog::set_level(spdlog::level::trace);

    if (argc < 3)
    {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string secretsFile{argv[1]};

    if (argv[2] == std::string{"exchange-info"})
    {
        if (argc != 3 && argc != 5)
        {
            std::cerr << "Usage: " << argv[0] << " secretsfile exchange-info [base quote]\n";
            return EXIT_FAILURE;
        }

        spdlog::info("Retrieving exchange information.");

        tradebot::Exchange exchange{getExchange(secretsFile)};

        if (argc == 3)
        {
            std::cout << exchange.getExchangeInformation().dump(4) << '\n';
        }
        else if (argc == 5)
        {
            std::cout << exchange.getExchangeInformation(ad::tradebot::Pair{argv[3], argv[4]}).dump(4) << '\n';
        }
    }
    else if (argv[2] == std::string{"filters"})
    {
        if (argc != 5)
        {
            std::cerr << "Usage: " << argv[0] << " secretsfile filters base quote\n";
            return EXIT_FAILURE;
        }

        spdlog::info("Retrieving filters for pair '{}{}'.", argv[3], argv[4]);

        tradebot::Exchange exchange{getExchange(secretsFile)};

        std::cout << exchange.queryFilters(ad::tradebot::Pair{argv[3], argv[4]}) << '\n';
    }
    else if (argv[2] == std::string{"average-price"})
    {
        if (argc != 5)
        {
            std::cerr << "Usage: " << argv[0] << " secretsfile average-price base quote\n";
            return EXIT_FAILURE;
        }

        spdlog::info("Retrieving average price for pair '{}{}'.", argv[3], argv[4]);

        tradebot::Exchange exchange{getExchange(secretsFile)};

        std::cout << exchange.getCurrentAveragePrice(ad::tradebot::Pair{argv[3], argv[4]}) << '\n';
    }
    else if (argv[2] == std::string{"buy"}
             || argv[2] == std::string{"sell"})
    {
        if (argc != 6)
        {
            std::cerr << "Usage: " << argv[0] << " secretsfile (buy|sell) base quote amount\n";
            return EXIT_FAILURE;
        }

        tradebot::Pair pair{argv[3], argv[4]};
        tradebot::Side side{tradebot::readSide(argv[2])};
        Decimal amount{argv[5]};

        spdlog::info("Placing market {} order for {} {}.",
                boost::lexical_cast<std::string>(side),
                amount,
                pair.base);

        tradebot::Trader trader{
            "placeorder",
            {argv[3], argv[4]},
            tradebot::Database{":memory:"},
            getExchange(secretsFile),
        };

        tradebot::Order order{
                trader.name,
                pair.base,
                pair.quote,
                amount,
                0.,
                side
        };

        trader.fillNewMarketOrder(order);

        spdlog::info("Executed at price: {} {}.", order.executionRate, order.quote);
    }
    else if (argv[2] == std::string{"query-order"})
    {
        if (argc != 6)
        {
            std::cerr << "Usage: " << argv[0] << " secretsfile query-order base quote client-id\n";
            return EXIT_FAILURE;
        }

        tradebot::Pair pair{argv[3], argv[4]};
        const std::string orderId{argv[5]};

        spdlog::info("Querying order {} for symbol {}.", orderId, pair.symbol());

        tradebot::Exchange exchange{getExchange(secretsFile)};

        binance::Response exchangeInfo =
            exchange.restApi.queryOrder(pair.symbol(), binance::ClientId{orderId});
        if (exchangeInfo.status == 200)
        {
            std::cout << exchangeInfo.json->dump(4) << '\n';
        }
        else
        {
            spdlog::critical("Could not query order.");
            return EXIT_FAILURE;
        }

    }
    else if (argv[2] == std::string{"account-info"})
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " secretsfile account-info\n";
            return EXIT_FAILURE;
        }

        tradebot::Exchange exchange{getExchange(secretsFile)};

        binance::Response accountInformation =
            exchange.restApi.getAccountInformation();
        if (accountInformation.status == 200)
        {
            std::cout << accountInformation.json->dump(4) << '\n';
        }
        else
        {
            spdlog::critical("Could not get account information.");
            return EXIT_FAILURE;
        }
    }
    else
    {
        std::cerr << "Unknown commande '" << argv[2] << "'.\n";
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
