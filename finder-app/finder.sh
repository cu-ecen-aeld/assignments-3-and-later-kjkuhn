#!/bin/bash

filesdir=$1
searchstr=$2

if [ $# -lt 2 ]
then
    echo "Usage: $0 <filesdir> <searchstr>"
    exit 1
fi
if [ ! -d "$filesdir" ]; then
    echo "Error: '$filesdir' is not a directory."
    exit 1
fi

file_count=$(find "$filesdir" -type f | wc -l)
occurences=0
while IFS= read -r -d '' file; do
    count=$(grep -c "$searchstr" "$file")
    occurences=$((occurences + count))
done < <(find "$filesdir" -type f -print0)


echo "The number of files are ${file_count} and the number of matching lines are ${occurences}"