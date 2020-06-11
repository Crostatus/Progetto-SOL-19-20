#!/bin/bash

logfile='log.txt'
clients=0
prefix='id :'
while read line; do
	if  [[ ${line} == id* ]] ;
	then
    clients=$((clients+1))
	fi
	echo $line
done < $logfile
echo "[BASH] Total clients served: $clients"
