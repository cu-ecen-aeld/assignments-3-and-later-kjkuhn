#!/bin/bash

writefile=$1
writestr=$2

if [ $# -lt 2 ]
then
    echo "Usage: $0 <writefile> <writestr>"
    exit 1
fi

directory=$(dirname $writefile)
if [ ! -d "$directory" ]; then
    mkdir -p $directory
    if [ $? -ne 0 ]; then
        echo "Error: Could not create directory"
        exit 1
    fi
fi

echo $writestr > $writefile

if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "Error: Could not write to file"
    exit 1
fi