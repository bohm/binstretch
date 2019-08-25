#!/bin/bash

usage()
{
	# echo "usage: ./build.sh BINS R S {INITIAL_SEQUENCE}"
	echo "usage: ./build.sh BINS R S {INITIAL_SEQUENCE} [-odir output-dir]"
}

if [ $# -lt 3 -o $# -eq 5 -o $# -gt 6 ]; then
	usage
	exit 1
fi

if [ $# -eq 3 ]; then
	IS="{}"
else 
	IS=$4
fi

if [ $# -eq 6 ]; then
    if [ $5 != "-odir" ]; then
      usage
      exit 1
    fi
fi

echo "Building for $1 bins, ratio $2/$3 and initial sequence $IS."

if [ $# -eq 3 -o $# -eq 4 ]; then
    echo "Running: mpic++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS main.cpp -o ../build/search-$1-bins-$2-$3 -pthread"
    cd algorithm; mpic++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS main.cpp -o ../build/search-$1-bins-$2-$3 -pthread; cd ..
    echo "Running: g++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS painter.cpp -o ../build/painter-$1-bins-$2-$3 -pthread"
    cd painter; g++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS painter.cpp -o ../build/painter-$1-bins-$2-$3 -pthread; cd ..
    echo "Running: g++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS rooster.cpp -o ../build/rooster-$1-bins-$2-$3 -pthread"
    cd painter; g++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS rooster.cpp -o ../build/rooster-$1-bins-$2-$3 -pthread; cd ..

else
    echo "Running: mpic++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS main.cpp -o $6/search-$1-bins-$2-$3 -pthread"
    cd algorithm; mpic++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS main.cpp -o $6/search-$1-bins-$2-$3 -pthread; cd ..
    echo "Running: g++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS painter.cpp -o $6/painter-$1-bins-$2-$3 -pthread"
    cd painter; g++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS painter.cpp -o $6/painter-$1-bins-$2-$3 -pthread; cd ..
    echo "Running: g++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS rooster.cpp -o $6/rooster-$1-bins-$2-$3 -pthread"
    cd painter; g++ -Wall -std=c++17 -O3 -march=native -D_BINS=$1 -D_R=$2 -D_S=$3 -D_I_S=$IS rooster.cpp -o $6/rooster-$1-bins-$2-$3 -pthread; cd ..
fi
