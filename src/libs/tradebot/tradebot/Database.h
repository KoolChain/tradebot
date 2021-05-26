#pragma once

#include "Order.h"

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

    void update(const Order & aOrder);
    void update(const Fragment & aFragment);

    Order getOrder(decltype(Order::id) aIndex);
    Fragment getFragment(decltype(Fragment::id) aIndex);
    std::size_t countFragments();
    std::vector<Fragment> getFragmentsComposing(const Order & aOrder);

    std::vector<Decimal> getSellRatesAbove(Decimal aRateLimit, const Pair & aPair);
    std::vector<Decimal> getBuyRatesBelow(Decimal aRateLimit, const Pair & aPair);

    void assignAvailableFragments(const Order & aOrder);

    Decimal sumFragmentsOfOrder(const Order & aOrder);

    //
    // High level API
    //
    Order prepareOrder(Side aSide,
                       Decimal aFragmentsRate,
                       const Pair & aPair,
                       Order::FulfillResponse aFulfillResponse);

private:
    std::experimental::propagate_const<std::unique_ptr<Impl>> mImpl;
};


} // namespace tradebot
} // namespace ad
