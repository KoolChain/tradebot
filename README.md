# Tradebot

Trading, without the emotion.

The recommended way to run this repository is via the provided [Docker application](##Docker).

## Authorizations

Access to Binance API must be authorized via an API key and a secret.
They should be provided to the application via a JSON file containing:

    {
        "server": "production", // or "test"
        "apikey": "your_api_key"
        "secretkey": "your_secret_key"
    }

### Binance (production) exchange

The tokens are obtained via the web application.

### Spot test network credentials

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

The test network credentials should be provided as CMake variables (done once at build folder level):

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

* Tradebot.
* A container periodically running `updatesheet.py`.

### Build

**Note**: Explicitly building the images is not required for deployments,
as docker compose will take care of it.

However, the command to build tradebot would be.

    docker build -t adnn/tradebot .

### Run

The one command to build containers and run the application:

    docker-compose up --build

By default, the application expects the following files to exist from the host working directory:

* `./secrets/binance.json`: The credentials to access Binance API
* `./secrets/googlesheet-tokens.json`: The user authorization for Google Sheet API access
* `./configs/dogebot.json`: The tradebot configuration
* `./configs/updatesheet.env`: `python/updatesheet.py` configuration

It mounts the corresponding folders at the root of the container.


#### Customization points

The expected path to the files above can be changed via environment variables that must be
set in the shell executing `docker-compose up`:

* BINANCE_SECRETS_FILE (default: `/secrets/binance.json`)
* DOGEBOT_CONFIG (default: `/configs/dogebot.json`)
* GOOGLE_TOKEN_FILE (default: `/secrets/googlesheet-tokens.json`)
* UPDATESHEET_CONFIG (default: `/configs/updatesheet.env`)

**Important**: This is convenient to change the _filename_ part of the path, to match the filename
on the host.
If the _dirname_ is changed, or if the folders are not named as expected on the host,
new mounts will also have to be provided.

## Production

Export the environment variables defined in [Customization points](###customization-points)
in the calling shell if their value should be overriden.

1. Gather secrets:
   * [Binance exchange API authorization](##Authorization) should be placed in `./secrets/binance.json`.
   * [Google Sheet API authorization](python/README.md##Authorization) should be placed in `./secrets/googlesheet-tokens.json`.

2. Provide the configuration for the services
   * For the Cpp trader, see the existing files in `./configs` for examples.
     The production config should be placed in `./configs/dogebot.json`.
   * For the python exporter, pace a file `./configs/updatesheet.env` with content:

        SPREADSHEET_ID=some_spreadsheet_id

3. The first time the application is deployed on a docker host, a new volume with an empty database
   is created. The database should be populated with initial fragments.
   The fragments will be spawned based following the Cpp trader configuration provided above.

    docker-compose run tradebot ./initial-fragments.sh

4. Then, launch the long-running stack.

    docker-compose up [--build #force build even if images already exist]
