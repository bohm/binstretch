# binstretch
*Computing lower bounds for Online Bin Stretching in C.*

This is a companion source code to the paper "Online Bin Stretching with Three Bins" by
[Martin Böhm](http://iuuk.mff.cuni.cz/~bohm/), [Jiří Sgall](http://iuuk.mff.cuni.cz/~sgall/),
[Rob van Stee](http://www.cs.le.ac.uk/people/rvs4/) and [Pavel Veselý](http:/iuuk.mff.cuni.cz/~vesely/), which is to be submitted
for publication. The code implements a minimax game tree search for lower bounds on Online Bin Stretching in C, using dynamic
programming and extensive caching.

The code is using a technique first presented in the following paper:

[Gabay, Brauner, Kotov -  Computing Lower Bounds for Semi-Online Optimization Problems: Application to the Bin Stretching](https://hal.archives-ouvertes.fr/hal-00921663)

The lower bound search of the paper is available [on Github](https://github.com/mgabay/Bin-Stretching-Lower-Bounds).

We build on their approach while improving the implementation both technically and conceptually, which allowed
us to find a better lower bound of 45/33 for three bins as well as a first bound of 19/14 for four bins and 
five bins. (Note that an updated version of the Gabay, Brauner Kotov from June 2015 independently shows the lower bound
of 19/14 for four bins as well.)
