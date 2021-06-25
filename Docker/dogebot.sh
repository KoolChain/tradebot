#!/usr/bin/env sh

echo "./dogebot ${BINANCE_SECRETS_FILE} productionbot /sqlite-data/productionbot.sqlite ${DOGEBOT_CONFIG}"
./dogebot ${BINANCE_SECRETS_FILE} productionbot /sqlite-data/productionbot.sqlite ${DOGEBOT_CONFIG}
