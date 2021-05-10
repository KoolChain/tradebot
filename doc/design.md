# Trade bot design

## Functional outline

### Initialization

1. At launch, list all "transitioning orders"
(i.e. `sending` or `cancelling`, but confirmation not received. There might have been a crash)
    * Update to their actual status

2. Cancel all active orders

3. Get the current rate.
    * Issue separate `BUY` **market** orders for all `BUY` constituants with target rates **above** current, grouped by target rate.
    * Issue separate `SELL` **market** orders for all `SELL` constituants with target rates **below** current, grouped by target rate.
    * Invoke fulfill callbacks, to distribute counter-order constituants.

   **Important**: The counter order will still be computed from the original target rates, not the actual fulfill rate.

### Main loop (2 orders active at the same time)

4. Place the `SELL` and `BUY` around the current rate.
  They emit *events* upon fulfilling.
  **Note**: This is the only moment the two active orders will be direct neighbors on the "rate scale".

5. Upon *fulfill event*:
    * distribute resulting counter-order constituants.
    * Cancel the other active order.
    * Place a `SELL` order at the rate step immediately **above** the fulfill rate
    * Place a `BUY` order at the rate step immediately **below** the fulfill rate
    * Loop on next *fulfill event*


## Reminders

Use decimals not floats

Fix a minimal order amount that makes sense. Take a look at the relation to "nominal" values.

Cut the space of rate as "change proportional" small ranges

Accumulate the orders for each value, at least while they are not posted as order on binance

Throttle when the server response with a back-off, but keep on reading successful orders to accumulate

The "order emitter" must be aware of the current rate:
* To send orders that are in the hot zone (because the zone moved, or because a new order was added in the current zone). Also cancel orders that get too far out from the hot zone?
* To sell (buy) at market every order that is below (above) the current rate. BUT keep track of the initial order rate, so we do no deplete a zone. This will be useful for both "back-off" and "bot was turned-off for some time" situations.

How to address websocket 24h transition (risk of missing an order fulfil maybe?) This is a more generic problem in case of crash, some orders might be missed: should it only be checked at launch though?

Handle 400 errors (such as 400 Client error -1021 "Timestamp for this request is outside of the recvWindow."). Probably retrying.

Mark all constituants as done once their order is fulfilled.


## Data model

Order
+ Client ID
+ Creation time
+ Direction (Buy / Sell)
+ Pair
+ Amount
+ Fulfill times (can be composed of several trades...)
+ Fulfill callback (the function that specifies the counter orders)
+ Status (Sending, Confirmed, Cancelling, Fulfilled)
+ Broker id (to match against Binance records)

Only in DB?
+ Constituants -> vector< pair<origin-order, amount> >
+ Fulfill spread ->  vector< pair<counter-order, amount> >
+ Fulfill taken-home (the amount taken out of the trade pool, into a wallet)
+ Fulfill rate
+ target rate (to be redeployed to the correct target rate in the live map of orders)


Permanent storage:
Orders active and inactive

## Statistics

* Partially filled orders
* Fees

Get stats for Day, Week, Month, Year, All:
* Money taken-home
* Money the could be re-invested
* Rate window
* Number of orders (Sell / Buy)
  * Orders fullfilled
  * Orders emitted
  * Orders cancelled
* Trade volume, in Doge and converted to $ and €
* * Longest buy-sell chain
* Backoff # and total time
* Crash count
