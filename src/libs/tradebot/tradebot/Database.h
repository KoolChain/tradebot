#pragma once

#include "Order.h"

#include <experimental/propagate_const>


namespace ad {
namespace tradebot {


class Database
{
    struct Impl;

public:
    Database(const std::string & aFilename);
    ~Database();

    long insert(const Order & aOrder);
    long insert(const Fragment & aFragment);

    Order getOrder(decltype(Order::id) aIndex);
    Fragment getFragment(decltype(Fragment::id) aIndex);

private:
    std::experimental::propagate_const<std::unique_ptr<Impl>> mImpl;
};


} // namespace tradebot
} // namespace ad
