#include <cstdio>
#include <climits>

#ifndef _CACHE_GUAR_LOCKS
#define _CACHE_GUAR_LOCKS

// Implementation of a guarantee cache with large entries that store the
// whole itemlists. First, a hash is compared, and if it is matched, a
// full check is done.

typedef std::array<short, S+1> itemlist;
const int GUAR_BLOCK_SIZE = 2048;

void print_itemlist(const itemlist& il)
{
    bool first = true;
    for (int j=1; j<=S; j++)
    {
	if (first)
	{
	    fprintf(stderr, "%02hd", il[j]);
	    first = false;
	} else {
	    if (j%10 == 1)
	    {
		fprintf(stderr, "|%02hd", il[j]);
	    } else {
		fprintf(stderr, ",%02hd", il[j]);
	    }
	}
    }


}


class guar_el_full
{
public:
    itemlist il;
    uint64_t ihash;
    bool feasible;

    inline void set(const binconf &b, bool f)
	{
	    feasible = f;
	    ihash = b.ihash();
	    il[0] = 0;
	    for (int i = 1; i <= S; i++)
	    {
		assert(b.items[i] >= SHRT_MIN && b.items[i] <= SHRT_MAX);
		il[i] = b.items[i];
	    }
	}
    inline bool value() const
	{
	    return feasible;
	}
    inline uint64_t hash() const
	{
	    return ihash;
	}
    inline bool empty() const
	{
	    return ihash == 0;
	}

    /*
    inline bool removed() const
	{
	    return ihash == 0;
	}

    */
    
    inline bool match(const guar_el_full& check) const
	{
	    if (check.ihash != ihash)
	    {
		return false;
	    }

	    // fprintf(stderr, "Hash matches for ");
	    // print_itemlist(il);
	    // fprintf(stderr, " and ");
	    // print_itemlist(check.il);
	    if (check.il != il) // array comparison.
	    {
		return false;
	    }

	    return true;
	}

    /*
    inline void remove()
	{
	    ihash = ZERO;
	}
    */

    inline void erase()
	{
	    ihash = 0;
	}
};

class guar_cache_locks
{
public:
    guar_el_full *ht;
    std::shared_mutex *locks;
    uint64_t htsize;
    int logsize;
    cache_measurements meas;

    void init_point(uint64_t point)
	{
	    ht[point].ihash = 0;
	}

    void init_block(int b, uint64_t start, uint64_t end)
	{
	    std::unique_lock l(locks[b]); // Lock.
	    for (uint64_t i = start; i < end; i++)
	    {
		init_point(i);
	    }
	    // Unlock.
	}

    // Compute the corresponding block for a given position.
    // Currently trivial, but it is better to use one function everywhere.
    int block(uint64_t pos) const
	{
	    return pos / GUAR_BLOCK_SIZE;
	}

    // Parameter logbytes: how many 2^bytes we are given for the cache.
    guar_cache_locks(uint64_t logbytes)
	{
	    assert(logbytes >= 0 && logbytes <= 64);

	    uint64_t bytes = two_to(logbytes);
	    const uint64_t megabyte = 1024 * 1024;
	    
	    htsize = power_of_two_below(bytes / sizeof(guar_el_full));
	    logsize = quicklog(htsize);
	    print_if<PROGRESS>("Given %llu logbytes (%llu MBs) and el. size %zu, set guar. cache (locks) to %llu els (logsize %llu).\n",
			    logbytes, bytes/megabyte, sizeof(guar_el_full), htsize, logsize);

	    ht = new guar_el_full[htsize];
	    assert(ht != NULL);

	    int blocks = (htsize / GUAR_BLOCK_SIZE) + 1;
	    locks = new std::shared_mutex[blocks];
 
	    uint64_t segment = GUAR_BLOCK_SIZE;
	    uint64_t start = 0;
	    uint64_t end = std::min(htsize, segment);

	    // Fix a limit of 32 threads for parallel memory allocation.
	    const int MEMTHREADS = 32;
	    std::array<std::thread, MEMTHREADS> th;
	    //std::vector<std::thread> th;
	    int w = 0;
	    while (w < blocks)
	    {
		for (int i = 0; i < MEMTHREADS; i++)
		{
		    // fprintf(stderr, "Processing block %d.\n", w);
		    if (w == blocks)
		    {
			break;
		    }
		    
		    th[i] = std::thread(&guar_cache_locks::init_block, this, w, start, end);
		    start += segment;
		    end += segment;
		    start = std::min(start, htsize);
		    end = std::min(end, htsize);
		    w++;
		}

		for (int i = 0; i < MEMTHREADS; i++)
		{
		    if (th[i].joinable())
		    {
			th[i].join();
		    }
		}
	    }
	}

    ~guar_cache_locks()
	{
	    delete ht;
	    delete locks;
	}

    uint64_t size()
	{
	    return htsize;
	}

    uint64_t trim(uint64_t ha)
	{
	    return logpart(ha, logsize);
	}

    
    void analysis();
    
    std::pair<bool, bool> lookup(const binconf& itemlist);
    void insert(const binconf& itemlist, const bool feasibility);

};

std::pair<bool, bool> guar_cache_locks::lookup(const binconf &itemlist)
{
    uint64_t hash = itemlist.ihash();
    uint64_t startpos = trim(hash);
    // int block_taken = -1;

    // Build a matching guar_el_full to speed up comparisons.
    // The boolean given to set() should not matter.
    guar_el_full reference; reference.set(itemlist, false);

    uint64_t limit = std::min(startpos + LINPROBE_LIMIT, htsize);
    // Use linear probing to check for the hashed value.
    for (uint64_t pos = startpos; pos < limit; pos++)
    {
	// Currently suboptimal -- as it locks and unlocks within the loop
	std::shared_lock l(locks[block(pos)]); // Lock.

	if (ht[pos].empty())
	{
	    MEASURE_ONLY(meas.lookup_miss_reached_empty++);
	    break; // Unlock by destruction.
	}

	if (ht[pos].match(reference))
	{
	    MEASURE_ONLY(meas.lookup_hit++);
	    return std::make_pair(true, ht[pos].value()); // Unlock by destruction.
	}

	l.unlock(); // Explicit unlock; might not be needed.
    }

    return std::make_pair(false, false);
}

void guar_cache_locks::insert(const binconf& itemlist, const bool feasibility)
{
    uint64_t hash = itemlist.ihash();
    uint64_t startpos = trim(hash);
    uint64_t limit = std::min(startpos + LINPROBE_LIMIT, htsize);
    
    // Build a matching guar_el_full to speed up comparisons.
    // The boolean given to set() should not matter.
    guar_el_full inserted; inserted.set(itemlist, feasibility);

    for (uint64_t pos = startpos; pos < limit; pos++)
    {
	// Currently *double* suboptimal, as we lock unique in advance.
	std::unique_lock l(locks[block(pos)]); // Lock.

	if (ht[pos].empty())
	{
	    MEASURE_ONLY(meas.insert_into_empty++);
	    ht[pos] = inserted;
	    return; // Unlock by destruction.
	}
	else if (ht[pos].match(inserted))
	{
	    MEASURE_ONLY(meas.insert_duplicate++);
	    return; // Unlock by destruction.
	}
	// else a different element is stored, we continue.

	l.unlock(); // Explicit unlock, might not be needed.
    }

    // if the cache is full, choose a random position
    uint64_t randpos = std::min(htsize, startpos + (rand() % LINPROBE_LIMIT) );
    std::unique_lock l(locks[block(randpos)]); // Lock.
    ht[randpos] = inserted;
    MEASURE_ONLY(meas.insert_randomly++);
    return; // Unlock by destruction.
}

void guar_cache_locks::analysis()
{
    for (uint64_t i = 0; i < htsize; i++)
    {
	if (ht[i].empty())
	{
	    meas.empty_positions++;
	} else
	{
	    meas.filled_positions++;
	}
    }
}

#endif // _CACHE_DP_LOCKS_HPP
