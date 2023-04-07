#!/usr/bin/env python3

import sys

def usage():
    print("Usage: ./showroot.py BINS R S [ISAND]")
    print("BINS -- number of bins")
    print("R -- size of the ALG bins (if volume R is packed, the lower bound R/S holds)")
    print("S -- size of the OPT bins")
    print("ISAND -- Initial sand on the first bin")

if __name__ == '__main__':
    if len(sys.argv) < 4:
        usage()
        exit(-1)
    bins: int = int(sys.argv[1])
    r: int = int(sys.argv[2])
    s: int = int(sys.argv[3])
    isand: int = 0
    if len(sys.argv) == 5:
        isand = int(sys.argv[4])

    if bins <= 0 or r <= 0 or s <= 0 or r < s:
        usage()
        exit(-1)

    loads = [str(isand)] + ((bins-1) * [str(0)])
    items = [str(isand)] + (s-1)*[str(0)]

    loadstr = "[" + " ".join(loads) + "]"
    itemstr = "(" + " ".join(items) + ")"
    last_sent: str = "0"
    if isand > 0:
        last_sent = "1"

    print(loadstr + " " + itemstr + " " + last_sent)
