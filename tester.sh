#!/bin/bash

echo "Starting SIGHUP test."
touch log.txt
./bin/supermarket &
sleep 25
pkill -f -HUP supermarket
echo "[BASH] SIGHUP sent."
inotifywait -q -q -e MODIFY log.txt
