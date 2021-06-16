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
