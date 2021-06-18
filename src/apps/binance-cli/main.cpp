#include <binance/Api.h>

#include <tradebot/Database.h>
#include <tradebot/Exchange.h>
#include <tradebot/Order.h>
#include <tradebot/Trader.h>

#include <spdlog/spdlog.h>

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

int main(int argc, char * argv[])
{
    spdlog::set_level(spdlog::level::trace);

    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile action args... \n"
            << "\taction might be: buy, sell, query-order, exchange-info\n"
            ;
        return EXIT_FAILURE;
    }

    const std::string secretsFile{argv[1]};

    if (argv[2] == std::string{"exchange-info"})
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " secretsfile exchange-info\n";
            return EXIT_FAILURE;
        }

        spdlog::info("Retrieving exchange information.");

        tradebot::Exchange exchange{getExchange(secretsFile)};

        binance::Response exchangeInfo = exchange.restApi.getExchangeInformation();
        if (exchangeInfo.status == 200)
        {
            std::cout << exchangeInfo.json->dump(4) << '\n';
        }
        else
        {
            spdlog::critical("Could not retrieve exchange information.");
            return EXIT_FAILURE;
        }
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

    return EXIT_SUCCESS;
}
