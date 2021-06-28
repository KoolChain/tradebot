# Statistics publisher

Statitics can be published to Google sheet via `updatesheet.py`.

The spreadsheet must be created externally (e.g. via the web app) and have the following sheets:
* `Orders`: columns A to N
* `Fragments`: columns A to I

**Important**: There is a 5M cell limit per spreadsheet, it is advised to remove unused columns
from the sheets above.

## Authorizations

To access a user's spreadsheet, this program reads tokens from a file (`tokens.json`)
that can be generated via `authorize.py`.

**Note**: `authorize.py` must be run from a graphical session, as it will open a web browser
to follow the oauth2 procedure.

`authorize.py` requires the applications credentials (`credentials.json`),
that can be obtained via the [Google Cloud Console](https://console.cloud.google.com/).

The authorization procedure for the application itself is detailed in
https://developers.google.com/sheets/api/quickstart/python


## Docker

This repository can build a docker image that will use `updatesheet.py`
to export the content of the database every 15 minutes.

**Important**: The recommended approach is to let [top-level Docker compose application](../README.md##Docker)
takes care of building and running the container.
The following instructions are development oriented.

### Cron based

Following: https://devopsheaven.com/cron/docker/alpine/linux/2017/10/30/run-cron-docker-alpine.html

#### Building image

From this folder run:

    docker build -t adnn/tradebot-cronexport .

#### Running the container

`--init` argument should be provided so the container can be stopped properly.

The container is configured via the following required environment variables:

* `SQLITE_FILE`: path the the SQLite3 database file.
* `GOOGLE_TOKEN_FILE`: path to a token file authorizing access to user spreadsheets (see: Authorize).
* `SPREADSHEET_ID`: ID of the spreadsheet used as export destination (extracted from its URL).

The pointed files must be accessible in the running container, usually via mounts.

e.g. command:

    docker run --init --rm --name tradebot-export \
        -e SPREADSHEET_ID=${some_spreadsheet_id} \
        -e GOOGLE_TOKEN_FILE=/token.json \
        -e SQLITE_FILE=/db.sqlite \
        -v $(pwd)/token.json:/token.json \
        -v $(pwd)/dogebot.sqlite:/db.sqlite \
        adnn/tradebot-cronexport

### Jobber based

Using: https://hub.docker.com/_/jobber

At the moment, there is a problem with capturing `stderr`:
https://github.com/dshearer/jobber/issues/331#issue-929952548

There is nonetheless a "test" image to build, with a test script `jobberprinter.py`
which outputs to both `stdout` and `stderr`.

#### Building image

From this folder run:

    docker build -t adnn/tradebot-jobberexport -f DockerfileJobber .

#### Running the container

    docker run --rm --name tradebot-export adnn/tradebot-jobberexport

## Development

### To revisit

* There is a 5M cells limit on each spreadsheet. This will cause perpetual errors once reached.
* The Docker container should notify (e.g. e-mail) when a run does not succeed.
  * `Jobber` instead of `cron` seemed promising, but I did not succeed in retrieving error messages.
* `updatesheet.py` operations are not atomic.
  * The `Orders` might be updated, but not the `Fragment`
  * The state might become inconsistent: the fragments are only retrieved for orders that were retrieved
    during the same run.
    In case of a run only updating orders, its fragments will not be exported on next run.

