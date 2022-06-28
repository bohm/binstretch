#!/bin/bash
BINS=7
R=19
S=14
ADVICE_PREFIX=""
TIME_SUFFIX=`date +"%Y-%m-%d %T"`

./build/listsaplings-$BINS-$R-$S --advice ./experiments/${ADVICE_PREFIX}advice-$BINS-$R-$S.txt > ./experiments/temporarylist-$BINS-$R-$S.txt 2>/dev/null

while read line; do
    echo "Processing sapling $line"
    echo "Processing sapling $line" >> "./logs/process-$BINS-$R-$S-$TIME_SUFFIX.txt"
    echo "$line" > ./experiments/temporarysapling.txt
    mpirun -np 2 --bind-to none "./build/search-$BINS-$R-$S" --root ./experiments/temporarysapling.txt </dev/null 2>&1 | tee -a "./logs/process-$BINS-$R-$S-$TIME_SUFFIX.txt"
done < ./experiments/temporarylist-$BINS-$R-$S.txt
