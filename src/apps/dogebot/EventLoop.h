#pragma once


#include <boost/asio/io_context.hpp>


namespace ad {


/// \brief An event loop implementation based on boost::Asio
class EventLoop
{
public:
    boost::asio::io_context & getContext();

    void run();

private:
    boost::asio::io_context ioContext;
};


inline boost::asio::io_context & EventLoop::getContext()
{
    return ioContext;
}


} // namespace ad
