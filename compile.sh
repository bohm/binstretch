#!/bin/bash

usage()
{
	echo "usage: ./build.sh BINS R S {INITIAL_SEQUENCE} [-odir output-dir] [--search/--painter/--rooster]"
}

if [ $# -lt 4 -o $# -gt 7 ]; then
	usage
	exit 1
fi

IS=$4

if [ "$5" == "-odir" ]; then
    OUTPUT=$6
else 
    OUTPUT="../build"
fi

BUILDING_ROOSTER=true
BUILDING_PAINTER=true
BUILDING_SEARCH=true

if [[ ( "$7" == "--search" ) || ( "$5" == "--search" ) ]]; then
	BUILDING_ROOSTER=false
	BUILDING_PAINTER=false
fi

if [[ ( "$7" == "--painter" ) || ( "$5" == "--painter" ) ]]; then
	BUILDING_SEARCH=false
	BUILDING_ROOSTER=false
fi

if [[ ( "$7" == "--rooster" ) || ( "$5" == "--rooster" ) ]]; then
	BUILDING_SEARCH=false
	BUILDING_PAINTER=false
fi

echo "Building for $1 bins, ratio $2/$3 and initial sequence $IS."

if [[ "$BUILDING_SEARCH" = true ]]; then
	echo "Running: mpic++ -Wall -std=c++17 -O3 -march=native -DIBINS=$1 -DIR=$2 -DIS=$3 -DII_S=$IS main.cpp -o $OUTPUT/search-$1-bins-$2-$3 -pthread"
	cd algorithm; mpic++ -Wall -std=c++17 -O3 -march=native -DIBINS=$1 -DIR=$2 -DIS=$3 -DII_S=$IS main.cpp -o $OUTPUT/search-$1-bins-$2-$3 -pthread; cd ..
fi

if [[ "$BUILDING_PAINTER" = true ]]; then
	echo "Running: g++ -Wall -std=c++17 -O3 -march=native -DIBINS=$1 -DIR=$2 -DIS=$3 -DII_S=$IS painter.cpp -o $OUTPUT/painter-$1-bins-$2-$3 -pthread"
	cd painter; g++ -Wall -std=c++17 -O3 -march=native -DIBINS=$1 -DIR=$2 -DIS=$3 -DII_S=$IS painter.cpp -o $OUTPUT/painter-$1-bins-$2-$3 -pthread; cd ..
fi

if [[ "$BUILDING_ROOSTER" = true ]]; then
    echo "Compiling converter binary for $1 bins and ratio $2/$3 bins into $OUTPUT/rooster-$1-$2-$3."
    cd rooster; g++ -I../ -Wall -std=c++17 -O3 -march=native -DIBINS=$1 -DIR=$2 -DIS=$3 -DII_S=$IS rooster.cpp -o $OUTPUT/rooster-$1-bins-$2-$3 -pthread; cd ..
fi
