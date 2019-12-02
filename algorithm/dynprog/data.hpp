#ifndef DYNPROG_DATA_HPP
#define DYNPROG_DATA_HPP

// A class which holds the small caches that the dynamic programming algorithm
// can use. It is expected that this class is stored inside the main computation
// thread.

struct dynprog_data
{
    std::vector<loadconf> *oldloadqueue;
    std::vector<loadconf> *newloadqueue;
    uint64_t *loadht;
    uint64_t largest_queue_observed = 0; // This is only used for measurements.
    
    dynprog_data()
	{
	    oldloadqueue = new std::vector<loadconf>();
	    oldloadqueue->reserve(LOADSIZE);
	    newloadqueue = new std::vector<loadconf>();
	    newloadqueue->reserve(LOADSIZE);
	    loadht = new uint64_t[LOADSIZE];
	}


    ~dynprog_data()
	{
	    delete oldloadqueue;
	    delete newloadqueue;
	    delete[] loadht;
	}
};

#endif
