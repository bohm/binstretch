#!/usr/bin/env python3

import sys

def usage():
    print("Usage: ./showroot.py BINS R S")
    print("BINS -- number of bins")
    print("R -- size of the ALG bins (if volume R is packed, the lower bound R/S holds)")
    print("S -- size of the OPT bins")

if __name__ == '__main__':
    if len(sys.argv) < 4:
        usage()
        exit(-1)
    bins: int = int(sys.argv[1])
    r: int = int(sys.argv[2])
    s: int = int(sys.argv[3])
    if bins <= 0 or r <= 0 or s <= 0 or r < s:
        usage()
        exit(-1)

    loads = bins * [str(0)]
    items = s*[str(0)]

    loadstr = "[" + " ".join(loads) + "]"
    itemstr = "(" + " ".join(items) + ")"
    last_sent: str = "0"
    print(loadstr + " " + itemstr + " " + last_sent)
