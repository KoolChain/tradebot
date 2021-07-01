# Tradebot

Trading, without the emotion.

The recommended way to run this repository is as a provided [Docker application](#docker).

## Introduction

**Tradebot** is the repository of a trading bot for [Binance](https://www.binance.com/) exchange.

It is composed of different [applications](src/apps) :

* `binance-cli`: a tool allowing some manipulations (placing orders) and queries (balance, ...)
from the command line.
* `dogebot`: the application to launch the trading bot. Several bots might be provided.
  * `naivebot`: a proof of concept, simply placing an order, and placing the reverse order
    (with some marging for earnings) upon completion.
  * `productionbot`: the current bot used on the production exchange.
* `initial-fragments`: populate a database with `Sell` fragments following the provided configuration.
  This is essential when first deploying a `productionbot`, since it only place orders for corresponding
  fragments.
* `tests`: unit, integratin, regression testing using Catch2 framework.
* `tradelist`: present a list of all the exchange's trades corresponding to a filled order.

The applications are implemented upon the following [libraries](src/libs):

* `binance`: implementation of the [Binance REST Api](https://binance-docs.github.io/apidocs).
* `trademath`: general mathematics functions and classes that can be applied to trading.
* `websocket`: a simple websocket library on top of Boost::Beast. Should be taken out of this repo.
* `tradebot`: relies on all the libraries above to provide the features required to implement a
  trading bot. It notably provides the `Trader` class, which is a high-level interface
  to interract with an exchange's REST Api and its websocket streams
  (currently only targeted at Binance).

It also contains [Python3 code](python/) to provide statistics of a running bot.
The main script is `updatesheets.py`, exporting the content of a database
to a Google Sheet spreadsheet.


### ProductionBot

**ProductionBot** realizes the original intent of this repository.

Functionally, it listens to the market stream for the latest trades, takes the current rate from
there, and check in the database if any fragment could be satisfied:

* `Sell` fragments with a target rate under the current rate.
* `Buy` fragments with a target rate above the current rate.

It then place limit FOK order(s) for the matching fragments (a separate order per side, per target rate).
Upon an order completion, the registered `Spawner` will be called upon each `Fragment`
composing the order, to spawn counter-fragments.

**Note**: Limit FOK has been chosen because it is guaranteed to either fill or not execute at all,
while allowing to set a limit.

Care has been taken so operation can be resumed in event of a crash, and that no irrecoverable
state should be lost.
(taking a few lessons from [crash-only software](https://en.wikipedia.org/wiki/Crash-only_software)).

Further details regarding the implementations can be found in [design.md](doc/design.md).

## Authorizations

Access to Binance API must be authorized via an API key and a secret.

They should be provided to the application via a JSON file containing:

    {
        "server": "production", // or "test"
        "apikey": "your_api_key",
        "secretkey": "your_secret_key"
    }

### Binance (production) exchange

The tokens are obtained via the web application: https://www.binance.com/en/my/settings/api-management

The restrictions should be edited to add `Enable Spot & Margin Trading`.

### Spot test network credentials

To use the testnet, API key and secret should be obtained from https://testnet.binance.vision/.
(As of this writing, it seems mandatory to log-in with Github).


## Usage

### Docker

There is a docker application (multi-container docker compose).
It starts both:

* `productionbot` (via `tradebot` application).
* A container periodically running `updatesheet.py`.

#### Build

**Note**: Explicitly building the images is not required for deployments,
as docker compose will take care of it when running.

However, the command to build tradebot would be.

    docker build -t adnn/tradebot .

#### Run

The one command to build containers and run the application:

    docker-compose up --build

By default, the application expects the following files and folder to
exist from the host working directory:

* `./secrets/binance.json`: The credentials to access Binance API
* `./secrets/googlesheet-tokens.json`: The user authorization for Google Sheet API access
* `./configs/dogebot.json`: The tradebot configuration
* `./configs/updatesheet.env`: `python/updatesheet.py` configuration
* `./logs/`: Folder where the bot will write its logs.

It mounts the corresponding folders at the root of the container.


##### Customization points

The expected path to the files above can be changed via environment variables that must be
set in the shell executing `docker-compose up`:

* **BINANCE_SECRETS_FILE** (default: `/secrets/binance.json`)
* **DOGEBOT_CONFIG** (default: `/configs/dogebot.json`)
* **GOOGLE_TOKEN_FILE** (default: `/secrets/googlesheet-tokens.json`)
* **UPDATESHEET_CONFIG** (default: `/configs/updatesheet.env`)

**Important**: This is convenient to change the _filename_ part of the path, to match the filename
on the host.
If the _dirname_ is changed, or if the folders are not named as expected on the host,
new mounts will also have to be provided.

The location where the container will write the bot logs on the host can be change via environment
variable:

* **BOT_LOG_FOLDER** (default: `./logs`)

### Production

Export the environment variables defined in [Customization points](#customization-points)
in the calling shell if their value should be overriden.

1. Gather secrets:
   * [Binance exchange API authorization](#authorizations) should be placed in `./secrets/binance.json`.
   * [Google Sheet API authorization](python/README.md#authorizations) should be placed in `./secrets/googlesheet-tokens.json`.

2. Provide the configuration for the services
   * For the Cpp trader, see the existing files in `./configs` for examples.
     The production config should be placed in `./configs/dogebot.json`.
   * For the python exporter, pace a file `./configs/updatesheet.env` with content:
   ```
   SPREADSHEET_ID=some_spreadsheet_id
   ```

3. The first time the application is deployed on a docker host, a new volume with an empty database
   is created. The database should be populated with initial fragments.
   The fragments will be spawned based following the Cpp trader configuration provided above.
   ```
   docker-compose run tradebot ./initial-fragments.sh
   ```

4. Then, launch the long-running stack.
   ```
   docker-compose up [--build #force build even if images already exist]
   ```


## Development

### Build

#### Prerequisites

* C++17 compiler
* git
* Conan (`pip3 install conan`)
* CMake *optional*

#### Procedure

For *Linux* or *macOS*

```
git clone --branch develop --recurse https://github.com/KoolChain/tradebot.git
cd tradebot
mkdir build && cd build
conan install --build missing -s build_type=Debug -o tradebot:build_tests=True ../conan
conan build -sf .. ../conan
```

#### Tests application

##### Providing testnet credentials

The test network credentials should be provided as CMake variables (done once at build folder level):

```
# Still from build/ folder
cmake -DBINANCE_TESTNET_APIKEY=${apikey} -DBINANCE_TESTNET_SECRETKEY=${secret} ..
# Build again, for example
conan build -sf .. ../conan
```

##### Running the tests

From `build/` folder

```
src/apps/tests/tests
```

### TODO

* Move `websocket` library to a more generic repository.
* Implement placing order with fixed quote quantity (currently only fixed base is implemented).
  * This would notably be beneficial for buy counter-fragments: currently `productionbot`
    decides how much quote should be re-invested in the buy counter-fragment, then have to convert
    it to the base amount at expected target rate. This might lead to missed opportunities in base
    space, if the order executes at a lower rate than target
    (currently providing "leftover quote" that was not invested to reach the fixed buy base amount,
    instead of spending the exact amount of quote and provide more base when the rate is lower).
* Clean bot shutdown. In particular, have it shutdown when Docker is stopping the container.
* Record more information when spawning counter-fragments:
  * Currently, when spawning counter-fragments, only the spawning `Order` is recorded. Maybe
    there could be an additional table recording the spawning `Fragment`s and the corresponding
    proportion (relative to the counter-fragment total).
    Note that counter-fragments have potentially several spawning `Fragment`s.
    (see [spawning relation](doc/design.md#spawning-relation))
* Ability to compose an order with fragments of different target rates.
  * This would allow to satisfy at once all beneficial fragments,
    instead of placing one order per rate.
  * It would not be possible anymore to have the `fragmentsRate` on `Order`.
  * This requires the above features of augmenting the spawning relation records when spawning
    (because the parent order would not allow to get the spawning fragment rate anymore).
* Investigate the cause for HTTP response with status 0
  * Is it related to cpr lib? Because it is using the system's Curl?
  * Should the application retry the query in this situation?
* Better handle order cancellation on the exchange:
  * Is it possible to specify the canceled client id?
  * In the user stream report, `c` vs `C` field.

