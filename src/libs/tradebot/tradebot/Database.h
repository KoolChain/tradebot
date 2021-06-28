#pragma once

#include "Fragment.h"
#include "Order.h"
#include "stats/Balance.h"
#include "stats/LaunchCount.h"

#include <experimental/propagate_const>


namespace ad {
namespace tradebot {


enum class Comparison
{
    Inferior,
    InferiorOrEqual,
    Superior,
    SuperiorOrEqual,
    Equal,
};


class Database
{
    struct Impl;

public:
    Database(const std::string & aFilename);
    ~Database();

    long insert(Order & aOrder);
    long insert(Order && aOrder)
    { return insert(aOrder); }

    long insert(Fragment & aFragment);
    long insert(Fragment && aFragment)
    { return insert(aFragment); }

    long insert(stats::Launch & aLaunch);
    long insert(stats::Launch && aLaunch)
    { return insert(aLaunch); }

    long insert(stats::Balance & aBalance);
    long insert(stats::Balance && aBalance)
    { return insert(aBalance); }

    void update(const Order & aOrder);
    void update(const Fragment & aFragment);

    Order & reload(Order & aOrder);
    Fragment & reload(Fragment & aFragment);

    Order getOrder(decltype(Order::id) aIndex);
    std::size_t countOrders();
    Fragment getFragment(decltype(Fragment::id) aIndex);
    std::size_t countFragments();

    std::size_t countBalances(MillisecondsSinceEpoch aStartingFrom = 0);

    std::vector<Fragment> getFragmentsComposing(const Order & aOrder);
    std::vector<Fragment> getUnassociatedFragments(Side aSide, Decimal aRate, const Pair & aPair);
    std::vector<Fragment> getUnassociatedFragments(Side aSide, const Pair & aPair);

    std::vector<Decimal> getSellRatesBelow(Decimal aRateLimit, const Pair & aPair);
    std::vector<Decimal> getBuyRatesAbove(Decimal aRateLimit, const Pair & aPair);

    /// \brief Will call either `getSellRatesBelow()` or `getBuyRatesAbove()` depending on `aSide`.
    std::vector<Decimal> getProfitableRates(Side aSide, Decimal aRateLimit, const Pair & aPair);

    void assignAvailableFragments(const Order & aOrder);

    /// \brief Sum the amounts of all fragments in the database.
    ///
    /// Notably usefull for tests.
    Decimal sumAllFragments();

    Decimal sumFragmentsOfOrder(const Order & aOrder);

    /// \brief Sum taken home for all fragments associated to the order.
    Decimal sumTakenHome(const Order & aOrder);

    std::vector<Order> selectOrders(const Pair & aPair, Order::Status aStatus);


    //
    // Transaction API
    //
    struct TransactionGuard
    {
        ~TransactionGuard();
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    [[nodiscard]] TransactionGuard startTransaction();

    void commit(TransactionGuard && aGuard);

    //
    // High level API
    //
    Order prepareOrder(const std::string & aTraderName,
                       Side aSide,
                       Decimal aFragmentsRate,
                       const Pair & aPair);

    void discardOrder(Order & aOrder);

    bool onFillOrder(const FulfilledOrder & aOrder);

private:
    std::experimental::propagate_const<std::unique_ptr<Impl>> mImpl;
};


} // namespace tradebot
} // namespace ad
