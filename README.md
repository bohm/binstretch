# binstretch
*Computing lower bounds for Online Bin Stretching in C++.*

This is a companion source code to the dissertation "Online Bin Stretching: Algorithms
and Computer Lower Bounds" by [Martin Böhm](http://iuuk.mff.cuni.cz/~bohm/).

The code implements a parallel minimax game tree search for lower bounds on
Online Bin Stretching in C++ and OpenMPI, using dynamic programming and
extensive caching.

The code is inspired by the original techniques of:

[Gabay, Brauner, Kotov -  Computing Lower Bounds for Semi-Online Optimization Problems: Application to the Bin Stretching](https://hal.archives-ouvertes.fr/hal-00921663)

The lower bound search of the paper is also available [on Github](https://github.com/mgabay/Bin-Stretching-Lower-Bounds).

