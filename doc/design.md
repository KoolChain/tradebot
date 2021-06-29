# Trade bot design

This design corresponds to the `productionbot` in `tradebot` application.

## Functional outline

### Initialization

**Note**: "transitioning orders" are `Cancelling` or `Sending` orders for which a crash occured
before the confirmation for the transition was received.
So they were not taken out of the transition status.

1. Clean-up `Inactive` orders (Orders that were created but never attempted to send to the exchange).

2. List all orders in either of these states: `Active`, `Sending` or `Canceling`. For each order:

    * Try to cancel it. This will cancel `Active` order that did not fulfill, as well as still active "transitioning orders" that did no fulfill either.
     (`Sending` that were received by the engine, and `Canceling` that were not received by the engine).

    * Query the order status on the exchange:
        * If it is `CANCELLED`, or `NOT EXISTING`
        (`Sending` that were not received, `Cancelling` that were received), mark it `Inactive` and clean-up the order.
        * If it is `FILLED`, query the trades to accumulate them and complete the order.

3. Start listening to the aggregate trade market stream (websocket)

### Main loop

Each time the market stream reports that a step on the predefined ladder of fragments rates is crossed:

4. From the reported trade rate.
    * Issue separate `BUY` orders for all `BUY` fragments with target rates **above** current, grouped by target rate.
    * Issue separate `SELL` orders for all `SELL` fragments with target rates **below** current, grouped by target rate.
   **Note**: The orders are issued as **limit FOK** orders, so they either immediately and completely fill, or are discarded.

5. Upon each order *fulfill*:
    * Distribute resulting counter-fragments
    * Complete order.

   **Important**: The counter-fragments should still be computed from the original target rates, not the actual fulfill rate.


## Operations outline

### Finding outstanding orders

List all fragments target rates for which there are fragments in of interest (GROUP BY `targetRate`)

  * Filter on `direction`.
  * Filter on > or < `targetRate`.
  * Fragments which are not associated to any order.

### Placing orders

For each rate:

  1. Create a new `Inactive` order in DB, get the `order id`. Set `fragmentsRate`.

  2. Assign all matching fragments to this `order id`.

  3. Sum all fragments amounts assigned to this order, and update `order amount`.

  4. Mark the order `Sending`.

  5. Send the order via binance API.

  6. Upon confirmation:

    * mark the order `Active`.
    * record the order creation time.
    * record the `exchange id`.

### Completing order

a. Spawn fragments (counter-fragments) by visiting each fragment associated to the completed order:
  * Record the completed fragments `takenHome`

b. Update the order in database:
  * Record `executionRate`
  * Record `fulfillTime`
  * Set state to `Fulfilled`.

### Cancelling order

a. Mark the order `Cancelling`.

b. Cancel the order via binance API.

c.  Upon confirmation:
  1. Mark the order `Inactive`.
  2. Clean-up


#### Clean-up Inactive order

a. Remove all fragments association.

b. Delete order from database.

## Old notes

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
