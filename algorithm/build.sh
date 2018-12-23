#!/bin/bash

usage()
{
	# echo "usage: ./build.sh BINS R S"
	echo "usage: ./build.sh BINS R S [-odir output-dir]"
}

if [ $# -lt 3 -o $# -eq 4 -o $# -gt 5 ]; then
	usage
	exit 1
fi

if [ $# -eq 5 ]; then
    if [ $4 != "-odir" ]; then
      usage
      exit 1
    fi
fi

echo "Building for $1 bins and ratio $2/$3."

if [ $# -eq 3 ]; then
    echo "Running: mpic++ -Wall -std=c++17 -O3 -march=core2 -D_BINS=$1 -D_R=$2 -D_S=$3 main.cpp -o ../tests/build-$1bins-$2-$3 -pthread"
    mpic++ -Wall -std=c++17 -O3 -march=core2 -D_BINS=$1 -D_R=$2 -D_S=$3 main.cpp -o ../tests/build-$1-bins-$2-$3 -pthread
else
    echo "Running: mpic++ -Wall -std=c++17 -O3 -march=core2 -D_BINS=$1 -D_R=$2 -D_S=$3 main.cpp -o $5/build-$1bins-$2-$3 -pthread"
    mpic++ -Wall -std=c++17 -O3 -march=core2 -D_BINS=$1 -D_R=$2 -D_S=$3 main.cpp -o $5/build-$1-bins-$2-$3 -pthread
fi
