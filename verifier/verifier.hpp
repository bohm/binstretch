#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <array>
#include <functional>
#include <algorithm>
#include <map>
#include <queue>

#define ERROR(...) fprintf(stderr, __VA_ARGS__); return -1;

#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__);
#define DEBUG_PRINT_VERTEX(x) x.print_info();
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINT_VERTEX(x) 
#endif

using namespace std;
const int BINS = 4;
const int R = 19; // capacity of a stretched bin
const int S = 14; // capacity of a unit bin

typedef long long unsigned int llu;

// data structures for the dynamic programming test
typedef std::array<int, BINS> LoadsArray;
typedef std::map< LoadsArray, bool> ConfigMap;
typedef std::queue< LoadsArray > ConfigQueue;

// helper function which prints a bin configuration
void print_array(const LoadsArray &ar)
{
    fprintf(stderr, "(");
    bool first = true;
    for (int i= 0; i<BINS; i++)
    {
	if(first)
	{
	    first = false;
	} else {
	    fprintf(stderr, ",");
	}
	
	fprintf(stderr, "%d", ar[i]);
    }
    fprintf(stderr, ")");
}

// Sets the array limits to a lexicographically first tuple.
void first_tuple(LoadsArray &limits)
{
    for(int bin = 0; bin < BINS; bin++)
    {
	limits[bin] = 0;
    }
}

// Checks if the array limits is the tuple (S,S,...,S).
bool is_last_tuple(const LoadsArray &limits)
{
    for(int bin = 0; bin < BINS; bin ++)
    {
	if (limits[bin] < S) {
	    return false;
	}
    }
    // It is (S,S,...,S):
    return true;
}

// Generates lexicographically next tuple; wraps around (this is never used).
void next_tuple(LoadsArray &limits)
{
    for( int bin = BINS-1; bin >=0; bin--)
    {
	if( limits[bin] < S)
	{
	    limits[bin]++;
	    break;
	} else {
	    limits[bin] = 0;
	}
    }
}

class Binconf {
public:
    LoadsArray loads; // bin 0, bin 1, bin 2, ...
    array<int, S+1> types; // types start at 1, end at S
    Binconf() {
	for (int i = 0; i < S+1; i++)
	{
	    types[i] = 0;
	}
    }; 

    // packs a new item, returns false if the packing creates a bin of size >= R
    bool pack(int item, int bin) {
	bool admissible = true;
	loads[bin] += item;

	if(loads[bin] >= R) {
	    admissible = false;
	}
	
	types[item]++;
	sort(loads.begin(), loads.end(), std::greater<int>());

	return admissible;
    };

    // Checks if the optimum can pack the list of items stored into binconf
    // into BINS bins of capacity S. Uses sparse dynamic programming, as described
    // in the paper.
    bool test() {
        ConfigQueue cur;
	ConfigQueue prev;
	ConfigMap cur_membership;
	LoadsArray config;
	LoadsArray newconf;
	DEBUG_PRINT("Testing feasibility by an offline optimum.\n");

	bool first_item = true;
	
	for(int type=1; type<=S; type++) {
	    int amount = types[type];
	    while(amount > 0) {
		if (first_item)
		{
		    for (int bin = 0; bin < BINS; bin++)
		    {
			config[bin] = 0;
		    }
		    config[0] = type;
		    cur.push(config);
		    cur_membership[config] = true;
		    first_item = false;
		} else {
		    while(!prev.empty())
		    {
			config = prev.front();
			prev.pop();
			// if the configuration is not yet in the current queue
			if(cur_membership.find(config) == cur_membership.end())
			{
			    for (int bin = 0; bin < BINS; bin++)
			    {
				if(config[bin] + type <= S)
				{
				    newconf = config;
				    newconf[bin] += type;
				    sort(newconf.begin(), newconf.end(), std::greater<int>());
				    if(cur_membership.find(newconf) == cur_membership.end())
				    {
					cur.push(newconf);
					cur_membership[newconf] = true;
				    }
	       
				}
			    }
			}
		    }
		}
		swap(cur, prev);
		cur_membership.clear();
		amount--;
	    }
	}

	// if no items arrived, it is trivially True
	if(first_item == true) {
	    return true;
	}
	// if queue is still nonempty, a packing is possible
	else if(! prev.empty())
	{
	    return true;
	} else {
	    return false;
	}
    }
    
    bool validate() {
	DEBUG_PRINT("Validating its bin configuration\n");

	int sumloads = 0, sumtypes = 0;
	for (auto i: loads) {
	    if(i > R || i < 0) return false;
	    sumloads += i;
	}

	for (unsigned int i=1; i<types.size(); i++) {
	    sumtypes += i * types[i];
	}
	
	if(sumloads != sumtypes) return false;

	return test();
    };
};

// Vertex of the game tree.
class Vertex {

public:
    /* We do not store pointers to children directly
     * because children are not known at the time of the parent's creation,
     * only their IDs. We store only IDs, at the cost of O(log n) access time.
     */
    vector<llu> children; 
    Binconf* configuration;
    int nextItem;
    llu id;
    
    Vertex(Binconf* c_) : configuration(c_) {};
    void fill_types();
    bool validate();
    bool recursive_validate();
    void print_info();
};

// the map of all vertices in the game tree, indexed by ids,
// which are produced by the lower bound generator
map<llu, Vertex> tree;


void Vertex::print_info()
{
    print_array(configuration->loads);
}

// recursively fills in the loads array; needs to be run
//   after the graph is complete

void Vertex::fill_types() {
    for(llu child_id: children) {
	Vertex& child = tree.at(child_id);
	std::copy(configuration->types.begin(), configuration->types.end(), child.configuration->types.begin());
	assert(nextItem <= S);
	DEBUG_PRINT("Filling next item %d into vertex %llu\n", nextItem, child_id);
	(child.configuration->types[nextItem])++;
	child.fill_types();
    }
}

    /* validate a vertex of the tree */
bool Vertex::validate() {
    DEBUG_PRINT("Validating vertex %llu\n", id);
    if(configuration == NULL) return false;
    if(configuration->validate() == false) return false;
	if(nextItem <= 0 || nextItem > S) return false;
	
	/* check that all three possible packings of nextItem into configuration are valid */
	bool found_all = true;
	for(int i = 0; i<BINS; i++)
	{
	    Binconf next_step(*configuration);
	    bool admissible = next_step.pack(nextItem,i);
	    if(!admissible) // skip this packing if it produces a load of size >= R
		continue;
	    
	    bool found = false;
	    for (llu child_id: children)
	    {
		const Vertex& child = tree.at(child_id); 

		/* If there is a vertex in the tree with the same bin configuration */
		for (auto const &keypair : tree)
		{
		    if (equal(next_step.types.begin(), next_step.types.end(), (keypair.second).configuration->types.begin())
			&& equal(next_step.loads.begin(), next_step.loads.end(), (keypair.second).configuration->loads.begin()))
		    {
			found = true;
			break;
		    }
		    
		}
		if (!found) {
		    DEBUG_PRINT("One of the valid children of vertex %llu was not found, namely: \n", id);
		    print_array(next_step.loads);
		    
		    found_all = false;
		    break;
		}
	    }
	}
	return found_all;
};

bool Vertex::recursive_validate() {
    if (!validate()) return false;
    for(llu child_id: children)
    {
	Vertex& child = tree.at(child_id);
	    if(!child.recursive_validate()) return false;
    }
    return true;
}

