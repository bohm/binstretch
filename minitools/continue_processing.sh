#!/bin/bash
# The same as process saplings, but does not edit temporarylist, instead only reads a list generated
# elsewhere (continuedlist-$BINS-$R-$S.txt).

BINS=12
R=19
S=14
ASSUME_STRING="--assume ./experiments/unsorted-assume-12-19-14.txt"
TIME_SUFFIX=`date +"%Y-%m-%d %T"`

while read line; do
    echo "Processing sapling $line"
    echo "Processing sapling $line" >> "./logs/process-$BINS-$R-$S-$TIME_SUFFIX.txt"
    echo "$line" > ./experiments/temporarysapling.txt
    mpirun -np 2 --bind-to none nice -n 10 "./build/search-$BINS-$R-$S" --root ./experiments/temporarysapling.txt $ASSUME_STRING </dev/null 2>&1 | tee -a "./logs/process-$BINS-$R-$S-$TIME_SUFFIX.txt"
done < ./experiments/continuedlist-$BINS-$R-$S.txt
