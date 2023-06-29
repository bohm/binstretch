#!/bin/bash
BINS=$1
R=$2
S=$3
LB=$4
RPRIME=$(($R-1))
ALPHA=$(($RPRIME-$S))
THRESHOLD=$((($S-2*$ALPHA)-1))
LOG="./logs/test-sand-$1-$2-$3.txt"

rm $LOG
./compile.sh $BINS $R $S --search 2>&1 | tee -a $LOG
for ((SAND = $LB; SAND <= $THRESHOLD; SAND++ ))
do
	echo "Testing sand $SAND/$THRESHOLD."
	./minitools/showroot.py $BINS $R $S $SAND   > ./experiments/test-sand-root.txt
	mpirun -np 2 -bind-to none nice -n 10 ./build/search-$BINS-$R-$S --root ./experiments/test-sand-root.txt 2>&1 | tee -a $LOG
done
