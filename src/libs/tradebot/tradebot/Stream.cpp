#include "Stream.h"


#include <spdlog/spdlog.h>


#include <boost/asio/post.hpp>


namespace ad {
namespace tradebot {


RefreshTimer::RefreshTimer(MaintenanceOperation aOperation, Duration aPeriod) :
    operation{std::move(aOperation)},
    period{std::move(aPeriod)},
    timer{
        ioContext,
        period
    },
    thread{
        [this]()
        {
            try
            {
                async_wait();
                ioContext.run();
            }
            catch (std::exception & aException)
            {
                spdlog::error("Refresh timer run was interrupted by exception: {}.",
                              aException.what());
            }

            if (! intendedClose)
            {
                spdlog::error("Refresh timer stopped without application consent.");
            }
        }
    }
{}


RefreshTimer::~RefreshTimer()
{
    // This object cannot be destroyed until the io_context is done running
    // so it is safe to capture it in a lambda executed on the io_context thread.
    boost::asio::post(
        timer.get_executor(),
        [this]
        {
            // Cancel any pending wait on the timer
            // (otherwise the io_context::run() would still block on already waiting handlers)
            timer.cancel();
            // There are situations where the timer handler might not be waiting anymore
            // even though the cancellation in happening in the same thread as the completion handler
            // (notably, completion handler already pending in the io_context queue, just after this lambda)
            // So it would not be cancelled by the call to cancel().
            // see: https://stackoverflow.com/a/43169596/1027706
            intendedClose = true;
        });

    thread.join(); // From this point, intendedClose is known to be `true`.
    spdlog::debug("Refresh timer successfully stopped.");
}


void RefreshTimer::onTimer(const boost::system::error_code & aErrorCode)
{
    if (aErrorCode == boost::asio::error::operation_aborted)
    {
        spdlog::debug("Refresh timer aborted.");
        return;
    }
    else if (aErrorCode)
    {
        spdlog::error("Error on refresh timer: {}. Will try to go on.", aErrorCode.message());
    }

    // timer closed value is changed in the same thread running io_context
    // so it cannot race,
    // and as importantly it cannot change value "in the middle" of the if body.
    if (! intendedClose)
    {
        operation();
        timer.expires_after(period);
        async_wait();
    }
    else
    {
        spdlog::debug("Timer was closed while the handler was already queued. Not restarting it.");
    }
}


void RefreshTimer::async_wait()
{
    // The io_context running the completion handled is a data membmer of `this`
    timer.async_wait(std::bind(&RefreshTimer::onTimer,
                               this,
                               std::placeholders::_1));
}


void onStreamReceive(const std::string & aMessage, Stream::ReceiveCallback aOnMessage)
{
    aOnMessage(Json::parse(aMessage));
}


Stream::Stream(WebsocketDestination aDestination,
               ReceiveCallback aOnMessage,
               UnintendedCloseCallback aOnUnintededClose,
               std::unique_ptr<RefreshTimer> aKeepAlive) :
    keepAlive{std::move(aKeepAlive)},
    websocket{
        // On connect
        [this]()
        {
            // Notify successful connection
            {
                std::scoped_lock<std::mutex> lock{mutex};
                status = Connected;
            }
            statusCondition.notify_one();
        },
        // On Message
        [onMessage = std::move(aOnMessage)](const std::string & aMessage)
        {
            onStreamReceive(aMessage, onMessage);
        }
    },
    websocketThread{
        [this,
         destination = std::move(aDestination),
         onUnintededClose = std::move(aOnUnintededClose)]()
        {
            try
            {
                websocket.run(destination.host, destination.port, destination.target);
            }
            catch (std::exception & aException)
            {
                spdlog::error("Websocket run was interrupted by exception: {}.", aException.what());
            }

            // Mark the stream as Done
            Status previousStatus;
            {
                std::scoped_lock<std::mutex> lock{mutex};
                previousStatus = status;
                status = Done;
            }
            // Will unlock openUserStream() in case the websocket never connected.
            statusCondition.notify_one();

            // Note: there is still potential for the websocket to disconnect between the moment
            // this flag is set to false and the moment websocket.async_close() does complete.
            // Although this would still be an unintended close situation, no special case is made
            // because the websocket not running anymore is what is wanted.
            if (! intendedClose)
            {
                if (previousStatus == Status::Connected)
                {
                    spdlog::warn("User data stream websocket closed without application consent.");
                }
                else if (previousStatus == Status::Initialize)
                {
                    spdlog::warn("User data stream websocket could not connect.");
                }
                onUnintededClose();
            }
        }
    }
{}


Stream::~Stream()
{
    // IMPORTANT: It is likely better to **never** close a listen key.
    // The listen key seems to be account-wide, being only one installed at any given time.
    // So, one application closing the listen key would close it for any other application also using it.
    //if (restApi.closeSpotListenKey(listenKey).status != 200)
    //{
    //    spdlog::critical("Cannot close the current spot listen key, cannot throw in a destructor.");
    //    // See note below.
    //    //websocket.async_close();
    //}

    // The close above should result in the websocket closing, so the thread terminating.
    // NOTE: I expected that closing the listen key would have the server close the websocket.
    //       Apparently this is not the case, so close it manually anyway.
    // NOTE: Anyway, the listen key is not closed anymore, the comments are kept to document the situation.

    intendedClose = true;
    websocket.async_close();
    websocketThread.join();
    spdlog::debug("Exchange stream successfully closed.");
}


} // namespace tradebot
} // namespace ad
