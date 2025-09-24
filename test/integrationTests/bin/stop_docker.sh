#!/bin/sh

cd `dirname $0`

docker compose -f "$COMPOSE_FILE_YML" down

