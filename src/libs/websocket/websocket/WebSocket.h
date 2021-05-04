#pragma once


#include <experimental/propagate_const>

#include <mutex>
#include <queue>


namespace ad {
namespace net {


    // Non movable, non copyable
class WebSocket
{
public:
    WebSocket();
    ~WebSocket();

    /// \attention Blocks the calling thread until the websocket is closed.
    void run(const std::string & aHost, const std::string & aPort);

private:
    struct impl;
    std::experimental::propagate_const<std::unique_ptr<impl>> mImpl;
};


} // namespace net
} // namespace ad
