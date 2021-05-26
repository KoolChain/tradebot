#include "Database.h"

#include "Order.h"

#include <sqlite_orm/sqlite_orm.h>

#include <spdlog/spdlog.h>


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

                       make_column("base", &Order::base),
                       make_column("quote", &Order::quote),
                       make_column("amount", &Order::amount),
                       make_column("fragments_rate", &Order::fragmentsRate),
                       make_column("execution_rate", &Order::executionRate),
                       make_column("side", &Order::side),
                       make_column("fulfill_response", &Order::fulfillResponse),
                       make_column("activation_time", &Order::activationTime),
                       make_column("status", &Order::status),
                       make_column("taken_home", &Order::takenHome),
                       make_column("fulfill_time", &Order::fulfillTime),
                       make_column("exchange_id", &Order::exchangeId)
            ),
            make_table("Fragments",
                       make_column("id", &Fragment::id, primary_key(), autoincrement()),

                       make_column("base", &Fragment::base),
                       make_column("quote", &Fragment::quote),
                       make_column("amount", &Fragment::amount),
                       make_column("target_rate", &Fragment::targetRate),
                       make_column("side", &Fragment::side),
                       make_column("spawning_order", &Fragment::spawningOrder),
                       make_column("composed_order", &Fragment::composedOrder)
            ));
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


long Database::insert(Fragment & aFragment)
{
    aFragment.id = mImpl->storage.insert(aFragment);
    spdlog::trace("Inserted fragment {} in database", aFragment.id);
    return aFragment.id;
}



void Database::update(const Order & aOrder)
{
    mImpl->storage.update(aOrder);
}


void Database::update(const Fragment & aFragment)
{
    mImpl->storage.update(aFragment);
}


Fragment Database::getFragment(decltype(Fragment::id) aIndex)
{
    return mImpl->storage.get<Fragment>(aIndex);
}


std::size_t Database::countFragments()
{
    return mImpl->storage.count<Fragment>();
}


std::vector<Decimal> Database::getSellRatesAbove(Decimal aRateLimit, const Pair & aPair)
{
    using namespace sqlite_orm;
    return mImpl->storage.select(&Fragment::targetRate,
            where(   (c(&Fragment::targetRate) > aRateLimit)
                  && (c(&Fragment::side) = static_cast<int>(Side::Sell))
                  && (c(&Fragment::base) = aPair.base)
                  && (c(&Fragment::quote) = aPair.quote)
                  && (c(&Fragment::composedOrder) = -1l) ), // Fragments not already part of an order
            group_by(&Fragment::targetRate)
            );
}


std::vector<Decimal> Database::getBuyRatesBelow(Decimal aRateLimit, const Pair & aPair)
{
    using namespace sqlite_orm;
    return mImpl->storage.select(&Fragment::targetRate,
            where(   (c(&Fragment::targetRate) < aRateLimit)
                  && (c(&Fragment::side) = static_cast<int>(Side::Buy))
                  && (c(&Fragment::base) = aPair.base)
                  && (c(&Fragment::quote) = aPair.quote)
                  && (c(&Fragment::composedOrder) = -1l) ), // Fragments not already part of an order
            group_by(&Fragment::targetRate)
            );
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


Decimal Database::sumFragmentsOfOrder(const Order & aOrder)
{
    using namespace sqlite_orm;
    std::unique_ptr<Decimal> result =
        mImpl->storage.sum(&Fragment::amount,
                where(is_equal(&Fragment::composedOrder, aOrder.id)));
    if (!result)
    {
        spdlog::critical("Cannot sum fragments for order {}", aOrder.id);
        throw std::logic_error("Unable to sum fragments.");
    }
    return *result;

    int id = 1;
    auto & storage = mImpl->storage;
}


std::vector<Fragment> Database::getFragmentsComposing(const Order & aOrder)
{
    using namespace sqlite_orm;
    return mImpl->storage.get_all<Fragment>(where(is_equal(&Fragment::composedOrder, aOrder.id)));
}


Order Database::prepareOrder(Side aSide,
                             Decimal aFragmentsRate,
                             const Pair & aPair,
                             Order::FulfillResponse aFulfillResponse)
{
    Order order{
        aPair.base,
        aPair.quote,
        0, // amount
        aFragmentsRate,
        0, // execution rate
        aSide,
        aFulfillResponse
    };

    auto transaction = mImpl->storage.transaction_guard();

    insert(order);
    assignAvailableFragments(order);
    order.amount = sumFragmentsOfOrder(order);
    update(order);

    transaction.commit();

    return order;
}

} // namespace tradebot
} // namespace ad