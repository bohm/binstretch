#!/bin/bash

usage()
{
	echo "usage: ./build.sh M T G [\"{INITIAL_SEQUENCE}\"] [-odir output-dir] [--search/--painter/--rooster] [--debug]"
	echo "where M: the number of bins/machines (e.g. 6)"
	echo "      T: the allowed load of bins (e.g. 19)"
	echo "      G: the optimal maximum load of all bins (e.g. 14)"
}

# First, parse the main three arguments.
if [[ $# -lt 3 ]]; then
    usage
    exit 1
fi

BINS=$1
R=$2
S=$3
I_S="{}"

# Default parameter values for other parameters:
OUTPUT="../build"
BUILDING_ROOSTER=true
BUILDING_PAINTER=true
BUILDING_SEARCH=true
OPTFLAG="-O3"

# Skip first three parameters, then iterate over the rest of the arguments.
shift 3
while (( "$#" )); do
    case "$1" in
	\{*)
	    I_S=$1
	    shift
	    ;;
	-odir)
	    OUTPUT=$2
	    shift 2
	    ;;
	--search)
	    BUILDING_ROOSTER=false
	    BUILDING_PAINTER=false
	    shift
	    ;;
	--painter)
	    BUILDING_SEARCH=false
	    BUILDING_ROOSTER=false
	    shift
	    ;;
	--rooster)
	    BUILDING_SEARCH=false
	    BUILDING_PAINTER=false
	    shift
	    ;;
	--debug)
	    OPTFLAG="-g3"
	    shift
	    ;;
	*) # unsupported flags
	    echo "Error: Unsupported flag $1" >&2
	    usage
	    exit 1
	    ;;
    esac
done

# If the directory OUTPUT does not exist, try to create it yourself.
if [ ! -d $OUTPUT ]; then
    mkdir $OUTPUT;
fi

if [[ "$I_S" == "{}" ]]; then
    echo "Building for $BINS bins and ratio $R/$S."
else
    echo "Building for $BINS bins, ratio $R/$S and initial sequence $I_S."
fi


if [[ "$BUILDING_SEARCH" = true ]]; then
	echo "Running: mpic++ -Wall -std=c++17 $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S main.cpp -o $OUTPUT/search-$BINS-$R-$S -pthread"
	cd algorithm; mpic++ -Wall -std=c++17 $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S main.cpp -o $OUTPUT/search-$BINS-$R-$S -pthread; cd ..
fi

if [[ "$BUILDING_PAINTER" = true ]]; then
	echo "Running: g++ -Wall -std=c++17 $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S painter.cpp -o $OUTPUT/painter-$BINS-$R-$S -pthread"
	cd painter; g++ -Wall -std=c++17 $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S painter.cpp -o $OUTPUT/painter-$BINS-$R-$S -pthread; cd ..
fi

if [[ "$BUILDING_ROOSTER" = true ]]; then
    echo "Compiling converter binary for $BINS bins and ratio $R/$S bins into $OUTPUT/rooster-$BINS-$R-$S."
    cd rooster; g++ -I../ -Wall -std=c++17 $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S rooster.cpp -o $OUTPUT/rooster-$BINS-$R-$S -pthread; cd ..
fi
