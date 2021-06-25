# Tradebot

Trading, without the emotion.

## Spot test network credentials

To use the testnet, API key and secret should be obtained from https://testnet.binance.vision/.
(As of this writing, it seems mandatory to log-in with Github).


## Build

### Prerequisites

* C++17 compiler
* git
* Conan (`pip3 install conan`)
* CMake *optional*

### Procedure

For *Linux* or *macOS*

```
git clone --branch develop --recurse https://github.com/KoolChain/tradebot.git
cd tradebot
mkdir build && cd build
conan install --build missing -s build_type=Debug -o tradebot:build_tests=True ../conan
conan build -sf .. ../conan
```

### Tests application

#### Providing testnet credentials

The test network credentials should be provided as CMake variables once

```
# Still from build/ folder
cmake -DBINANCE_TESTNET_APIKEY=${apikey} -DBINANCE_TESTNET_SECRETKEY=${secret} ..
# Build again, for example
conan build -sf .. ../conan
```

#### Running the tests

From `build/` folder

```
src/apps/tests/tests
```

## Docker

There is a docker application (multi-container docker compose).
It starts both:

* Tradebot
* A recurrent `updatesheet.py` container

### Build

**Note**: Separately building the images is not required, as docker compose will take care of it.

However, the command to build tradebot would be.

    docker build -t adnn/tradebot .

### Run

The one command to build containers and run the application:

    docker-compose up --build

By default, the application expects the following files to exist from the host working directory:

* `./secrets/binance.json`: The credentials to access Binance API
* `./configs/dogebot.json`: The tradebot configuration

It mounts the corresponding folders at the root of the container.


#### Customization points

The expected path to the files above can be changed via environment variables that must be
set in the shell executing `docker-compose up`:

* BINANCE_SECRETS_FILE (default: `/secrets/binance.json`)
* DOGEBOT_CONFIG (default: `/configs/dogebot.json`)
