#!/bin/sh


trap 'echo "Exiting .."; kill $(pgrep) aesdsock; exit 0' SIGINT

if [ -e "aesdsock" ] then
    echo "starting aesd sock"
    aesdsock -d
else
    echo "aesdsock not found"
    exit(-1)
fi

while true; do
    sleep 1
done