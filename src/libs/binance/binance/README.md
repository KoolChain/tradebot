## Questions

* Difference between HTTP 418 and 403?
* What is a 504 exactly?
* Is there a type of order that either entirely fill or not at all?
  * That would be a `AON` (All Or None), and it does not seem to be available at all in the API.
* What are filters for?
  * They are conditions that must be met by orders to be accepted by the binance engine.
* What is an iceberg order?
* What is the difference between wapi (about to be decommisionned) and sapi? And api?


## API endpoints

Base:
* https://api.binance.com

If there are performance issues with the endpoint above, these API clusters are also available:

* https://api1.binance.com
* https://api2.binance.com
* https://api3.binance.com

### Test network

Apparently, only supports `api` endpoints, [not `sapi`/`wapi`](https://dev.binance.vision/t/which-streams-and-endpoints-are-available-on-testnet/3414/2?u=shredastaire).

## Headers

### Request

* `X-MBX-APIKEY`: The API key.
* `signature`


### Response

* X-MBX-USED-WEIGHT-(intervalNum)(intervalLetter)
* X-MBX-ORDER-COUNT-(intervalNum)(intervalLetter)

Note: `X-MBX-USED-WEIGHT-(intervalNum)(intervalLetter)` is present at least on api/v3, but apparently not on sapi/v1 (to confirm?).
There is also still the legacy `X-MBX-USED-WEIGHT`, which seems to give the same value (but the key does not contain the interval).

* Retry-After: back-off, or time until ban is lifted.

## Parameters

For `GET`, as query string.

For `POST`, `PUT`, `DELETE`, can also be in the request body with content type `application/x-www-form-urlencoded`.

* timestamp: milliseconds of request creation, **required** for signed endpoints.
* recvWindow(5000): duration of validity for the request. Optional.

## Signature

HMAC SHA256 signature, with `secretKey` as key, and `totalParams` as the value, encoded as hexadecimal string.

* `totalParams`: the query string concatenated with the request body

Signature is **not** case sensitive.


## Websocket streams

Base:
* wss://stream.binance.com:9443

### User Data Streams

#### Order Update payload

* (lowercase) `x` is the current execution type, which seems to be a duplication of the current order status
unless when it is `TRADE` (i.e., a trade occured for part of all of the order).
* (uppercase) `X` is the current order status. I suppose it will always be equal to `x`,
unless when `x` is `TRADE`, then `X` should be either `PARTIALLY_FILLED` or `FILLED` (for the laste trade).
