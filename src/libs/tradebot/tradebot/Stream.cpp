#include "Stream.h"


#include <spdlog/spdlog.h>


#include <boost/asio/post.hpp>


namespace ad {
namespace tradebot {


//const std::chrono::minutes LISTEN_KEY_REFRESH_PERIOD{30};
const std::chrono::minutes LISTEN_KEY_REFRESH_PERIOD{10};
// Good for testing
//const std::chrono::milliseconds LISTEN_KEY_REFRESH_PERIOD{100};


void onStreamReceive(const std::string & aMessage, StreamCallback aOnExecutionReport)
{
    Json json = Json::parse(aMessage);
    if (json.at("e") == "executionReport")
    {
        aOnExecutionReport(std::move(json));
    }
}


// IMPORTANT: Will execute the HTTP PUT query on the timer io_context thread
// So it introduces concurrent execution of http requests
// (potentially complicating proper implementation of "quotas observation and waiting periods").
// TODO potentially have it post the request to be executed on the main thread.
void Stream::onListenKeyTimer(const boost::system::error_code & aErrorCode)
{
    if (aErrorCode == boost::asio::error::operation_aborted)
    {
        spdlog::debug("Listen key refresh timer aborted.");
        return;
    }
    else if (aErrorCode)
    {
        spdlog::error("Error on listen key refresh timer: {}. Will try to go on.", aErrorCode.message());
    }

    // timer closed value is changed in the same thread running io_context
    // so it cannot race,
    // and as importantly it cannot change value "in the middle" of the if body.
    if (! intendedClose)
    {
        restApi.pingSpotListenKey();
        keepListenKeyAlive.expires_after(LISTEN_KEY_REFRESH_PERIOD);
        keepListenKeyAlive.async_wait(std::bind(&Stream::onListenKeyTimer,
                                                this,
                                                std::placeholders::_1));
    }
    else
    {
        spdlog::debug("Timer was closed while the handler was already queued. Not restarting it.");
    }
}


Stream::Stream(binance::Api & aRestApi, StreamCallback aOnExecutionReport) :
    restApi{aRestApi},
    listenKey{restApi.createSpotListenKey().json->at("listenKey")},
    keepListenKeyAlive{
        listenKeyIoContext,
        LISTEN_KEY_REFRESH_PERIOD
    },
    listenKeyThread{
        [this]()
        {
            listenKeyIoContext.run();
        }
    },
    websocket{
        // On connect
        [this]()
        {
            {
                std::scoped_lock<std::mutex> lock{mutex};
                status = Connected;
            }
            statusCondition.notify_one();

            // Cannot be done at the start of the thread directly
            // because it would still block the run when the websocket cannot connect
            keepListenKeyAlive.async_wait(std::bind(&Stream::onListenKeyTimer,
                                                    this,
                                                    std::placeholders::_1));

        },
        // On Message
        [onReport = std::move(aOnExecutionReport)](const std::string & aMessage)
        {
            onStreamReceive(aMessage, onReport);
        }
    },
    websocketThread{
        [this]()
        {
            try
            {
                websocket.run(restApi.getEndpoints().websocketHost,
                              restApi.getEndpoints().websocketPort,
                              "/ws/" + listenKey);

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
            // this flag is set to false and the moment websocket.async_close() is called.
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
            }
        }
    }
{}


Stream::~Stream()
{
    // This object cannot be destroyed until the io_context is done running
    // so it is safe to capture it in a lambda executed on the io_context thread.
    boost::asio::post(
        keepListenKeyAlive.get_executor(),
        [this]
        {
            // Cancel any pending wait on the timer
            // (otherwise the io_context::run() would still block on already waiting handlers)
            keepListenKeyAlive.cancel();
            // There are situations where the timer handler might not be waiting anymore
            // even though the cancellation in happening in the same thread as the completion handler
            // (notably, completion handler already pending in the io_context queue, just after this lambda)
            // So it would not be cancelled by the call to cancel().
            // see: https://stackoverflow.com/a/43169596/1027706
            intendedClose = true;
        });

    // IMPORTANT: It is likely better to **never** close a listen key.
    // The listen key seems to be account-wide, being only one installed at any given time.
    // So, one application closing the listen key would close it for any other application also using it.
    //if (restApi.closeSpotListenKey(listenKey).status != 200)
    //{
    //    spdlog::critical("Cannot close the current spot listen key, cannot throw in a destructor.");
    //    // See note below.
    //    //websocket.async_close();
    //}

    listenKeyThread.join(); // From this point, intendedClose is known to be `true`.

    // The close above should result in the websocket closing, so the thread terminating.
    // NOTE: I expected that closing the listen key would have the server close the websocket.
    //       Apparently this is not the case, so close it manually anyway.
    // NOTE: Anyway, the listen key is not closed anymore, the comments are kept to document the situation.
    websocket.async_close();
    websocketThread.join();
    spdlog::debug("User stream closed successfully.");
}



} // namespace tradebot
} // namespace ad
