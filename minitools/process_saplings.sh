#!/bin/bash
BINS=12
R=19
S=14
ROOT_STRING=""
ADVICE_STRING="--advice ./experiments/kibbitz-twelve.txt"
ASSUME_STRING=""
SEARCH_SUFFIX="-0mon"
TIME_SUFFIX=`date +"%Y-%m-%d %T"`

nice -n 10 ./build/listsaplings-$BINS-$R-$S $ROOT_STRING $ADVICE_STRING $ASSUME_STRING > ./experiments/temporarylist-$BINS-$R-$S.txt

while read line; do
    echo "Processing sapling $line"
    echo "Processing sapling $line" >> "./logs/process-$BINS-$R-$S-$TIME_SUFFIX.txt"
    echo "$line" > ./experiments/temporarysapling.txt
    mpirun -np 2 --bind-to none nice -n 10 "./build/search-$BINS-$R-$S$SEARCH_SUFFIX" --root ./experiments/temporarysapling.txt $ASSUME_STRING </dev/null 2>&1 | tee -a "./logs/process-$BINS-$R-$S-$TIME_SUFFIX.txt"
done < ./experiments/temporarylist-$BINS-$R-$S.txt
