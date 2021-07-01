#!/usr/bin/env sh

echo "./binance-cli ${BINANCE_SECRETS_FILE} $@"
./binance-cli ${BINANCE_SECRETS_FILE} $@
