#!/bin/bash

usage()
{
	echo "usage: ./build.sh M T G [-odir output-dir] [--search/--painter/--rooster/--kibbitzer/--minitools/--tests] [--debug] [--older]"
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
OUTPUT="build"
BUILDING_ROOSTER=true
BUILDING_PAINTER=true
BUILDING_KIBBITZER=true
BUILDING_SEARCH=true
BUILDING_MINITOOLS=true
BUILDING_TESTS=true
CPP_STANDARD="c++2a"
LINKING_SUFFIX=""
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
	    BUILDING_KIBBITZER=false
	    BUILDING_MINITOOLS=false
	    BUILDING_TESTS=false
	    shift
	    ;;
	--painter)
	    BUILDING_SEARCH=false
	    BUILDING_ROOSTER=false
	    BUILDING_KIBBITZER=false
	    BUILDING_MINITOOLS=false
	    BUILDING_TESTS=false
	    shift
	    ;;
	--rooster)
	    BUILDING_SEARCH=false
	    BUILDING_PAINTER=false
	    BUILDING_KIBBITZER=false
	    BUILDING_MINITOOLS=false
	    BUILDING_TESTS=false
	    shift
	    ;;
	--kibbitzer)
	    BUILDING_SEARCH=false
	    BUILDING_PAINTER=false
	    BUILDING_ROOSTER=false
	    BUILDING_MINITOOLS=false
	    BUILDING_TESTS=false
	    shift
	    ;;
	--minitools)
	    BUILDING_SEARCH=false
	    BUILDING_PAINTER=false
	    BUILDING_ROOSTER=false
	    BUILDING_KIBBITZER=false
	    BUILDING_TESTS=false
	    shift
	    ;;
	--tests)
	    BUILDING_SEARCH=false
	    BUILDING_PAINTER=false
	    BUILDING_ROOSTER=false
	    BUILDING_KIBBITZER=false
	    BUILDING_MINITOOLS=false
	    shift
	    ;;
    
	--older)
	    LINKING_SUFFIX="-lstdc++fs"
	    shift
	    ;;
	--debug)
	    OPTFLAG="-O3 -g"
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
	echo "Running: mpic++ -I./ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S main.cpp -o ../$OUTPUT/search-$BINS-$R-$S -pthread $LINKING_SUFFIX"
	cd search; mpic++ -I./ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S main.cpp -o ../$OUTPUT/search-$BINS-$R-$S -pthread $LINKING_SUFFIX; cd ..
fi

if [[ "$BUILDING_PAINTER" = true ]]; then
	echo "Running: g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S painter.cpp -o ../$OUTPUT/painter-$BINS-$R-$S -pthread $LINKING_SUFFIX"
	cd painter; g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S painter.cpp -o ../$OUTPUT/painter-$BINS-$R-$S -pthread $LINKING_SUFFIX; cd ..
fi

if [[ "$BUILDING_KIBBITZER" = true ]]; then
	echo "Running: g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S kibbitzer.cpp -o ../$OUTPUT/kibbitzer-$BINS-$R-$S -pthread $LINKING_SUFFIX"
	cd kibbitzer; g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S kibbitzer.cpp -o ../$OUTPUT/kibbitzer-$BINS-$R-$S -pthread $LINKING_SUFFIX; cd ..
fi

if [[ "$BUILDING_ROOSTER" = true ]]; then
    echo "Compiling converter binary for $BINS bins and ratio $R/$S bins into $OUTPUT/rooster-$BINS-$R-$S."
    cd rooster; g++ -I../ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S rooster.cpp -o ../$OUTPUT/rooster-$BINS-$R-$S -pthread $LINKING_SUFFIX; cd ..
fi

if [[ "$BUILDING_MINITOOLS" = true ]]; then
	echo "Running: g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S listsaplings.cpp -o ../$OUTPUT/listsaplings-$BINS-$R-$S -pthread $LINKING_SUFFIX"
	cd minitools; g++ -I../search/ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S listsaplings.cpp -o ../$OUTPUT/listsaplings-$BINS-$R-$S -pthread $LINKING_SUFFIX; cd ..
	echo "Running: g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native minitools/alg-winning-table.cpp -o ./$OUTPUT/awt -pthread"
	g++ -I./search/ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native minitools/alg-winning-table.cpp -o ./$OUTPUT/awt -pthread

fi

if [[ "$BUILDING_TESTS" = true ]]; then
    echo "Compiling $OUTPUT/generationtest-$BINS-$R-$S."
    cd tests; g++ -I../search/ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S generationtest.cpp -o ../$OUTPUT/generationtest-$BINS-$R-$S $LINKING_SUFFIX; cd ..
fi
