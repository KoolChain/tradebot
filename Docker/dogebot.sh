#!/usr/bin/env sh

echo "./dogebot ${BINANCE_SECRETS_FILE} ${DOGEBOT_CONFIG} productionbot /sqlite-data/productionbot.sqlite"
./dogebot ${BINANCE_SECRETS_FILE} ${DOGEBOT_CONFIG} productionbot /sqlite-data/productionbot.sqlite
