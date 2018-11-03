#!/bin/bash

usage()
{
	echo "usage: ./build.sh BINS R S"
	# echo "usage: ./build.sh BINS R S [-o output-file]"
}

if [ $# -lt 3 ]; then
	usage
	exit 1
fi

if [ $# -gt 4 ]; then
	usage
	exit 1
fi

echo "Building for $1 bins and ratio $2/$3."
echo "Running: mpic++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 main.cpp -o ../tests/build-$1bins-$2-$3 -pthread"
mpic++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 main.cpp -o ../tests/build-$1-bins-$2-$3 -pthread
