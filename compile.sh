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
ISCALE=0

# Default parameter values for other parameters:
OUTPUT="build"
OUTPUT_SUBFOLDER="build/$BINS-$R-$S"
BUILDING_ROOSTER=true
BUILDING_PAINTER=true
BUILDING_KIBBITZER=true
BUILDING_SEARCH=true
BUILDING_MINITOOLS=true
BUILDING_TESTS=true
CPP_STANDARD="c++2a"
LINKING_SUFFIX=""
OPTFLAG="-O3 -DNDEBUG"
SCALE_FLAG=""
# OPTFLAG="-O3"

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
	--scale)
	    SCALE_FLAG="-DISCALE=$2"
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

if [ ! -d $OUTPUT_SUBFOLDER ]; then
    mkdir $OUTPUT_SUBFOLDER;
fi

if [[ "$I_S" == "{}" ]]; then
    echo "Building for $BINS bins and ratio $R/$S."
else
    echo "Building for $BINS bins, ratio $R/$S and initial sequence $I_S."
fi

if [ ! -d cmake-build-release ]; then
	mkdir cmake-build-release
fi

if [[ "$BUILDING_SEARCH" = true ]]; then
   cd cmake-build-release || exit; cmake .. -DIBINS=$BINS -DIR=$R -DIS=$S "$SCALE_FLAG"; cmake --build ./ --target search -- -j 22; cmake --build ./ --target search -- -j 22; cd ..
# 	echo "Running: g++ -I./ -I../../parallel-hashmap/ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S main.cpp -o ../$OUTPUT_SUBFOLDER/search -pthread $LINKING_SUFFIX"
# 	cd search || exit; g++ -I./ -I../../parallel-hashmap/ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S main.cpp -o ../$OUTPUT_SUBFOLDER/search -pthread $LINKING_SUFFIX; cd ..
fi

if [[ "$BUILDING_PAINTER" = true ]]; then
	echo "Running: g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S painter.cpp -o ../$OUTPUT_SUBFOLDER/painter -pthread $LINKING_SUFFIX"
	cd painter || exit; g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S painter.cpp -o ../$OUTPUT_SUBFOLDER/painter -pthread $LINKING_SUFFIX; cd ..
fi

if [[ "$BUILDING_KIBBITZER" = true ]]; then
	echo "Running: g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S kibbitzer.cpp -o ../$OUTPUT_SUBFOLDER/kibbitzer -pthread $LINKING_SUFFIX"
	cd kibbitzer || exit; g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S kibbitzer.cpp -o ../$OUTPUT_SUBFOLDER/kibbitzer -pthread $LINKING_SUFFIX; cd ..
fi

if [[ "$BUILDING_ROOSTER" = true ]]; then
    echo "Compiling converter binary for $BINS bins and ratio $R/$S bins into $OUTPUT/rooster-$BINS-$R-$S."
    cd rooster || exit; g++ -I../ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S rooster.cpp -o ../$OUTPUT/rooster-$BINS-$R-$S -pthread $LINKING_SUFFIX; cd ..
fi

if [[ "$BUILDING_MINITOOLS" = true ]]; then
  if [[ $ISCALE -eq 0 ]]; then
    echo "The parameter --scale needs to be specified for minitools compilation."
    exit 1
  fi
#	echo "Running: g++ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S listsaplings.cpp -o ../$OUTPUT/listsaplings-$BINS-$R-$S -pthread $LINKING_SUFFIX"
#	cd minitools; g++ -I../search/ -I../../parallel-hashmap/ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DII_S=$I_S listsaplings.cpp -o ../$OUTPUT/listsaplings-$BINS-$R-$S -pthread $LINKING_SUFFIX; cd ..
   cd cmake-build-debug || exit; cmake .. -DIBINS=$BINS -DIR=$R -DIS=$S "$SCALE_FLAG"; cmake --build ./ --target minibs-tests -- -j 22; cd ..

	g++ -I./search/ -I../parallel-hashmap/  -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DI_SCALE=$ISCALE tests/minibs-tests.cpp -o ./$OUTPUT_SUBFOLDER/minibs-tests-$ISCALE -pthread
  echo "Compiling ./$OUTPUT_SUBFOLDER/awt-$ISCALE".
	g++ -I./search/ -I../parallel-hashmap/  -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DI_SCALE=$ISCALE minitools/alg-winning-table.cpp -o ./$OUTPUT_SUBFOLDER/awt-$ISCALE -pthread
  echo "Compiling ./$OUTPUT_SUBFOLDER/all-losing-$ISCALE."
	g++ -I./search/ -I../parallel-hashmap/  -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DI_SCALE=$ISCALE minitools/all-losing.cpp -o ./$OUTPUT_SUBFOLDER/all-losing-$ISCALE -pthread
  echo "Compiling ./$OUTPUT_SUBFOLDER/sand-on-ab-$ISCALE."
	g++ -I./search/ -I../parallel-hashmap/  -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S -DI_SCALE=$ISCALE minitools/sand-on-ab.cpp -o ./$OUTPUT_SUBFOLDER/sand-on-ab-$ISCALE -pthread
fi

if [[ "$BUILDING_TESTS" = true ]]; then
    echo "Compiling $OUTPUT/generationtest-$BINS-$R-$S."
    cd tests || exit; g++ -I../search/ -Wall -std=$CPP_STANDARD $OPTFLAG -march=native -DIBINS=$BINS -DIR=$R -DIS=$S generationtest.cpp -o ../$OUTPUT/generationtest-$BINS-$R-$S $LINKING_SUFFIX; cd ..
fi
