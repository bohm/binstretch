# binstretch
*Computing lower bounds for Online Bin Stretching in C++.*

This is a companion source code to the dissertation "Online Bin Stretching: Algorithms
and Computer Lower Bounds" by [Martin Böhm](https://sites.google.com/cs.uni.wroc.pl/boehm/).

The code implements a parallel minimax game tree search for lower bounds on
Online Bin Stretching in C++ and OpenMPI, using dynamic programming and
extensive caching.

The code is inspired by the original techniques of:

[Gabay, Brauner, Kotov -  Computing Lower Bounds for Semi-Online Optimization Problems: Application to the Bin Stretching](https://hal.archives-ouvertes.fr/hal-00921663)

The lower bound search of the paper is also available [on Github](https://github.com/mgabay/Bin-Stretching-Lower-Bounds).

(The version submitted alongside the dissertation is up to the commit `6ee81dd5d4f71d4eec5cf42194b59c447fe0bafa`.)

# Lower bounds for Online Bin Stretching

This table captures the current state of the art for lower bounds on the stretching factor for Online Bin Stretching
as of 2023-06-29. The table also contains unpublished results. Please let me know if any new results are published.

The term *granularity* indicates parameters of the lower bound search or construction. For example, if the granularity is
164/120, all items appearing in the lower bound are multiples of 1/120.

| # of bins | Lower Bound   | Granularity | Status                  | Comment                                                                                            |
|-----------|---------------|-------------|-------------------------|----------------------------------------------------------------------------------------------------|
| 2         | 4/3 ~ 1.333   | 4/3         | published, verified     | Bound is tight, classical result.                                                                  |
| 3         | 41/30 ~ 1.366 | 164/120     | unpublished, verified   | Results in preparation (Böhm), verified by Coq.                                                    |
| 4 - 5     | 19/14 ~ 1.357 | 19/14       | published, verified     | Böhm, Simon: https://www.sciencedirect.com/science/article/pii/S030439752200593X                   | 
| 6 - 8     | 15/11 ~ 1.363 | 15/11       | preprint, verified      | Lhomme, Romane, Catusse, Brauner: https://arxiv.org/abs/2207.04931, verified by Coq independently. |
| 9-12      | 19/14 ~ 1.357 | 19/14       | unpublished, unverified | Results in preparation (Böhm), currently not verified.                                             |
| 13+       | 4/3 ~ 1.333   | 4/3         | published, verified     | Classical lower bound.                                                                             | 
