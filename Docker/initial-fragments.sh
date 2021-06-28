#!/usr/bin/env sh

echo "./initial-fragments ${BINANCE_SECRETS_FILE} /sqlite-data/productionbot.sqlite ${DOGEBOT_CONFIG}"
./initial-fragments ${BINANCE_SECRETS_FILE} /sqlite-data/productionbot.sqlite ${DOGEBOT_CONFIG}
