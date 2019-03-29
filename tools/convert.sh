#!/bin/bash
# Converts a file from the bin stretching lower bound search program
# into a valid DOT file.

usage()
{
    echo "Usage: ./dotify.sh file.gen"
}

if [ $# -lt 1 -o $# -gt 2 ]; then
	usage
	exit 1
fi

INFILE=$1
DOTFILE=${1/%gen/dot}
echo "Generating $DOTFILE from $INFILE."
sed -r "s/label=\"([^\"]*)\"(.*),heur=\"([^\"]*)\"/label=\"\1h:\3\"\2/" < $INFILE > $DOTFILE
