#include "EventLoop.h"


#include <spdlog/spdlog.h>


namespace ad {


void EventLoop::run()
{
    // io_context run returns as soon as there is no work left.
    // create a timer never expiring, so it keeps on waiting for events to post work.
    boost::asio::deadline_timer runForever{ioContext, boost::posix_time::pos_infin};
    runForever.async_wait([](const boost::system::error_code & aErrorCode)
        {
            spdlog::error("Interruption of run forever timer: {}.", aErrorCode.message());
        }
    );

    ioContext.run();
}


} // namespace ad
