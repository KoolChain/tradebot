#include "Database.h"

#include "Logging.h"

#include <sqlite_orm/sqlite_orm.h>

#include "OrmAdaptors-impl.h"


namespace ad {
namespace tradebot {

namespace detail {

auto initializeStorage(const std::string & aFilename)
{
    using namespace sqlite_orm;
    return make_storage(
            aFilename,
            make_table("Orders",
                       make_column("id", &Order::id, primary_key(), autoincrement()),

                       make_column("trader_name", &Order::traderName),
                       make_column("base", &Order::base),
                       make_column("quote", &Order::quote),
                       make_column("amount", &Order::amount),
                       make_column("fragments_rate", &Order::fragmentsRate),
                       make_column("side", &Order::side),
                       make_column("activation_time", &Order::activationTime),
                       make_column("status", &Order::status),
                       make_column("fulfill_time", &Order::fulfillTime),
                       make_column("execution_rate", &Order::executionRate),
                       make_column("commission", &Order::commission),
                       make_column("commission_asset", &Order::commissionAsset),
                       make_column("exchange_id", &Order::exchangeId)
            ),
            make_table("Fragments",
                       make_column("id", &Fragment::id, primary_key(), autoincrement()),

                       make_column("base", &Fragment::base),
                       make_column("quote", &Fragment::quote),
                       make_column("amount", &Fragment::amount),
                       make_column("target_rate", &Fragment::targetRate),
                       make_column("side", &Fragment::side),
                       make_column("taken_home", &Fragment::takenHome),
                       make_column("spawning_order", &Fragment::spawningOrder),
                       make_column("composed_order", &Fragment::composedOrder)
            ),
            make_table("Launches",
                       make_column("id", &stats::Launch::id, primary_key(), autoincrement()),

                       make_column("time", &stats::Launch::time)
            ),
            make_table("Balances",
                       make_column("id", &stats::Balance::id, primary_key(), autoincrement()),

                       make_column("time", &stats::Balance::time),
                       make_column("base_balance", &stats::Balance::baseBalance),
                       make_column("quote_balance", &stats::Balance::quoteBalance),
                       make_column("base_buy_potential", &stats::Balance::baseBuyPotential),
                       make_column("quote_buy_potential", &stats::Balance::quoteBuyPotential),
                       make_column("base_sell_potential", &stats::Balance::baseSellPotential),
                       make_column("quote_sell_potential", &stats::Balance::quoteSellPotential)
            )
            );
}

} // namespace detail



struct Database::Impl
{
    using Storage = decltype(detail::initializeStorage(""));

    Impl(const std::string & aFilename);

    Storage storage;
};


Database::Impl::Impl(const std::string & aFilename) :
    storage{detail::initializeStorage(aFilename)}
{
    // Does not seem to add a timeout
    //storage.busy_timeout(50000);

    // Allows to have reader without locking the one writer
    // see: https://www.sqlite.org/wal.html
    storage.pragma.journal_mode(sqlite_orm::journal_mode::WAL);
    storage.sync_schema();
}


Database::Database(const std::string & aFilename) :
    mImpl{std::make_unique<Database::Impl>(aFilename)}
{}


Database::~Database()
{}


long Database::insert(Order & aOrder)
{
    aOrder.id = mImpl->storage.insert(aOrder);
    spdlog::trace("Inserted order {} in database", aOrder.id);
    return aOrder.id;
}


Order Database::getOrder(decltype(Order::id) aIndex)
{
    return mImpl->storage.get<Order>(aIndex);
}


std::size_t Database::countOrders()
{
    return mImpl->storage.count<Order>();
}


long Database::insert(Fragment & aFragment)
{
    aFragment.id = mImpl->storage.insert(aFragment);
    spdlog::trace("Inserted fragment {} in database", aFragment.id);
    return aFragment.id;
}


long Database::insert(stats::Launch & aLaunch)
{
    aLaunch.id = mImpl->storage.insert(aLaunch);
    spdlog::trace("Inserted launch {} in database", aLaunch.id);
    return aLaunch.id;
}


long Database::insert(stats::Balance & aBalance)
{
    aBalance.id = mImpl->storage.insert(aBalance);
    spdlog::trace("Inserted balance {} in database", aBalance.id);
    return aBalance.id;
}


void Database::update(const Order & aOrder)
{
    mImpl->storage.update(aOrder);
}


void Database::update(const Fragment & aFragment)
{
    mImpl->storage.update(aFragment);
}


Order & Database::reload(Order & aOrder)
{
    aOrder = getOrder(aOrder.id);
    return aOrder;
}


Fragment & Database::reload(Fragment & aFragment)
{
    aFragment = getFragment(aFragment.id);
    return aFragment;
}


Fragment Database::getFragment(decltype(Fragment::id) aIndex)
{
    return mImpl->storage.get<Fragment>(aIndex);
}


std::size_t Database::countFragments()
{
    return mImpl->storage.count<Fragment>();
}


std::size_t Database::countBalances(MillisecondsSinceEpoch aStartingFrom)
{
    using namespace sqlite_orm;
    return mImpl->storage.count<stats::Balance>(where(c(&stats::Balance::time) >= aStartingFrom));
}


std::vector<Decimal> Database::getSellRatesBelow(Decimal aRateLimit, const Pair & aPair)
{
    using namespace sqlite_orm;
    return mImpl->storage.select(&Fragment::targetRate,
            where(   (c(&Fragment::targetRate) <= aRateLimit)
                  && (c(&Fragment::side) = static_cast<int>(Side::Sell))
                  && (c(&Fragment::base) = aPair.base)
                  && (c(&Fragment::quote) = aPair.quote)
                  && (c(&Fragment::composedOrder) = -1l) ), // Fragments not already part of an order
            group_by(&Fragment::targetRate)
            );
}


std::vector<Decimal> Database::getBuyRatesAbove(Decimal aRateLimit, const Pair & aPair)
{
    using namespace sqlite_orm;
    return mImpl->storage.select(&Fragment::targetRate,
            where(   (c(&Fragment::targetRate) >= aRateLimit)
                  && (c(&Fragment::side) = static_cast<int>(Side::Buy))
                  && (c(&Fragment::base) = aPair.base)
                  && (c(&Fragment::quote) = aPair.quote)
                  && (c(&Fragment::composedOrder) = -1l) ), // Fragments not already part of an order
            group_by(&Fragment::targetRate)
            );
}


std::vector<Decimal> Database::getProfitableRates(Side aSide,
                                                  Decimal aRateLimit,
                                                  const Pair & aPair)
{
    switch(aSide)
    {
        case Side::Sell:
            return getSellRatesBelow(aRateLimit, aPair);
        case Side::Buy:
            return getBuyRatesAbove(aRateLimit, aPair);
        default:
            throw std::domain_error{"Invalid Side enumerator."};
    }
}


void Database::assignAvailableFragments(const Order & aOrder)
{
    using namespace sqlite_orm;
    // I think there is a bug when trying to compile some clauses.
    // see: https://github.com/fnc12/sqlite_orm/issues/723
    mImpl->storage.update_all(set(c(&Fragment::composedOrder) = aOrder.id),
            where(
                     //(c(&Fragment::targetRate) = aOrder.fragmentsRate) // compilation error
                  is_equal(&Fragment::targetRate, aOrder.fragmentsRate)
                  && (c(&Fragment::side) = static_cast<int>(aOrder.side))
                  && (c(&Fragment::base) = aOrder.base)
                  && (c(&Fragment::quote) = aOrder.quote)
                  && (c(&Fragment::composedOrder) = -1l) ) // Fragments not already part of an order
            );
}


Decimal Database::sumAllFragments()
{
    using namespace sqlite_orm;
    std::unique_ptr<Decimal> result = mImpl->storage.sum(&Fragment::amount);
    if (!result)
    {
        // there are not fragments -> the sum is zero
        return 0;
    }
    return fromFP(*result);
}


Decimal Database::sumFragmentsOfOrder(const Order & aOrder)
{
    using namespace sqlite_orm;
    std::unique_ptr<Decimal> result =
        mImpl->storage.sum(&Fragment::amount,
                where(is_equal(&Fragment::composedOrder, aOrder.id)));
    if (!result)
    {
        spdlog::critical("Cannot sum fragments for order {}.", aOrder.id);
        throw std::logic_error("Unable to sum fragments.");
    }
    // Note: Surprisingly, calling sum on the amount Decimal field does not go through
    // row_extractor<ad::Decimal>. So we fix for exact precision explicitly.
    return fromFP(*result);
}


Decimal Database::sumTakenHome(const Order & aOrder)
{
    using namespace sqlite_orm;
    std::unique_ptr<Decimal> result =
        mImpl->storage.sum(&Fragment::takenHome,
                where(is_equal(&Fragment::composedOrder, aOrder.id)));
    if (!result)
    {
        spdlog::critical("Cannot sum taken home for fragments composing order {}.", aOrder.id);
        throw std::logic_error("Unable to sum taken home.");
    }
    // Note: Surprisingly, calling sum on the amount Decimal field does not go through
    // row_extractor<ad::Decimal>. So we fix for exact precision explicitly.
    return fromFP(*result);
}


std::vector<Fragment> Database::getFragmentsComposing(const Order & aOrder)
{
    using namespace sqlite_orm;
    if (aOrder.id == -1)
    {
        throw std::logic_error{"Cannot get fragments composing an order not matched in the database."};
    }
    return mImpl->storage.get_all<Fragment>(where(is_equal(&Fragment::composedOrder, aOrder.id)));
}


std::vector<Fragment> Database::getUnassociatedFragments(Side aSide,
                                                         Decimal aRate,
                                                         const Pair & aPair)
{
    using namespace sqlite_orm;
    return mImpl->storage.get_all<Fragment>(
            where(is_equal(&Fragment::composedOrder, -1l)
                  && is_equal(&Fragment::base, aPair.base)
                  && is_equal(&Fragment::quote, aPair.quote)
                  && is_equal(&Fragment::side, static_cast<int>(aSide))
                  && is_equal(&Fragment::targetRate, aRate)));
}


std::vector<Fragment> Database::getUnassociatedFragments(Side aSide,
                                                         const Pair & aPair)
{
    using namespace sqlite_orm;
    return mImpl->storage.get_all<Fragment>(
            where(is_equal(&Fragment::composedOrder, -1l)
                  && is_equal(&Fragment::base, aPair.base)
                  && is_equal(&Fragment::quote, aPair.quote)
                  && is_equal(&Fragment::side, static_cast<int>(aSide))));
}


std::vector<Order> Database::selectOrders(const Pair & aPair, Order::Status aStatus)
{
    using namespace sqlite_orm;
    return mImpl->storage.get_all<Order>(
            where(is_equal(&Order::status, static_cast<int>(aStatus))
                  && (c(&Order::base) = aPair.base)
                  && (c(&Order::quote) = aPair.quote)
            ));
}


struct Database::TransactionGuard::Impl
{
    decltype(Database::mImpl->storage.transaction_guard()) guard;
};


Database::TransactionGuard::~TransactionGuard() = default;

Database::TransactionGuard Database::startTransaction()
{
    return TransactionGuard{std::unique_ptr<TransactionGuard::Impl>{
        new TransactionGuard::Impl{mImpl->storage.transaction_guard()},
    }};
}


void Database::commit(TransactionGuard && aGuard)
{
    aGuard.mImpl->guard.commit();
}


Order Database::prepareOrder(const std::string & aTraderName,
                             Side aSide,
                             Decimal aFragmentsRate,
                             const Pair & aPair)
{
    Order order{
        aTraderName,
        aPair.base,
        aPair.quote,
        Decimal{"0"}, // amount
        aFragmentsRate,
        aSide,
    };

    auto transaction = mImpl->storage.transaction_guard();

    insert(order);
    assignAvailableFragments(order);
    order.amount = sumFragmentsOfOrder(order);
    update(order);

    transaction.commit();

    return order;
}


void Database::discardOrder(Order & aOrder)
{
    using namespace sqlite_orm;

    auto transaction = mImpl->storage.transaction_guard();

    mImpl->storage.update_all(set(c(&Fragment::composedOrder) = -1l),
                              where(is_equal(&Fragment::composedOrder, aOrder.id)));
    mImpl->storage.remove<Order>(aOrder.id);

    // Bug 2021/05/27: I don't think the sqlite_orm implementation of the transactions works,
    // commenting out this commit(), the order is still removed from DB
    transaction.commit();

    spdlog::trace("Discarded order {} from database.", aOrder.id);
    aOrder.id = -1;
}


bool Database::onFillOrder(const FulfilledOrder & aOrder)
{
    using namespace sqlite_orm;

    // Make it resilient to multiple "fulfill" notifications.
    // e.g. Realizing the order is fulfilled while trying to cancel it, and receiving
    // the WebSocket notif at the same time.
    // Note: this works well only if onFillOrder() is called from a single thread.
    bool alreadyFilled = (getOrder(aOrder.id).status == Order::Status::Fulfilled);

    if (! alreadyFilled)
    {
        update(aOrder);
        spdlog::trace("Order '{}' is marked fulfilled.",
                      aOrder.getIdentity());
    }
    else
    {
        spdlog::warn("Order '{}' was already fulfilled.",
                      aOrder.getIdentity());
    }

    return ! alreadyFilled;
}

} // namespace tradebot
} // namespace ad
