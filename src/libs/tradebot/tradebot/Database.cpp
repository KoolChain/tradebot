#include "Database.h"

#include "Order.h"

#include "sqlite_orm/sqlite_orm.h"

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
                       make_column("direction", &Order::direction),
                       make_column("creation_time", &Order::creationTime),
                       make_column("fulfill_response", &Order::fulfillResponse),
                       make_column("status", &Order::status),
                       make_column("taken_home", &Order::takenHome),
                       make_column("fulfill_time", &Order::fulfillTime)
            ),
            make_table("Fragments",
                       make_column("id", &Fragment::id, primary_key(), autoincrement()),

                       make_column("base", &Fragment::base),
                       make_column("quote", &Fragment::quote),
                       make_column("amount", &Fragment::amount),
                       make_column("target_rate", &Fragment::targetRate),
                       make_column("direction", &Fragment::direction),
                       make_column("origin_order", &Fragment::originOrder),
                       make_column("status", &Fragment::status)
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


long Database::insert(const Order & aOrder)
{
    auto id = mImpl->storage.insert(aOrder);
    spdlog::trace("Inserted order {} in database", id);
    return id;
}


Order Database::getOrder(decltype(Order::id) aIndex)
{
    return mImpl->storage.get<Order>(aIndex);
}


long Database::insert(const Fragment & aFragment)
{
    auto id = mImpl->storage.insert(aFragment);
    spdlog::trace("Inserted fragment {} in database", id);
    return id;
}


Fragment Database::getFragment(decltype(Fragment::id) aIndex)
{
    return mImpl->storage.get<Fragment>(aIndex);
}


} // namespace tradebot
} // namespace ad
