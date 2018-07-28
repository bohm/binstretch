#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstdint>
#include <cinttypes>
#include <cassert>
#include <vector>
#include <array>
#include <functional>
#include <algorithm>
#include <map>
#include <queue>

// The variables below are given as command line parameters.
unsigned int BINS = 0, R = 0, S = 0; // R = capacity of a stretched bin, S = capacity of an optimal bin.

const bool DEBUG = false;

typedef long long unsigned int llu;

// data structures for the dynamic programming test
//typedef std::array<int, BINS> loads_array;
typedef std::vector<unsigned int> loads_array;
typedef std::map<loads_array, bool> config_map;
typedef std::queue<loads_array> config_queue;

uint64_t root_id = 0;

// helper function which prints a bin configuration
void print_array(const loads_array &ar)
{
    assert(ar.size() == BINS);

    fprintf(stderr, "(");
    bool first = true;
    for (unsigned int bin = 0; bin < BINS; bin++)
    {
	if (first)
	{
	    first = false;
	} else {
	    fprintf(stderr, ",");
	}
	
	fprintf(stderr, "%u", ar[bin]);
    }
    fprintf(stderr, ")");
}

template <bool PARAM> void print(const char *format, ...)
{
    if (PARAM)
    {
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stderr, format, argptr);
	va_end(argptr);
    }
}

void ERROR(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);

    abort();
}

// Sets the array limits to a lexicographically first tuple.
void first_tuple(loads_array &limits)
{
    assert(limits.size() == 0);
    
    for(int bin = 0; bin < BINS; bin++)
    {
	limits.push_back(0);
    }
}

// Checks if the array limits is the tuple (S,S,...,S).
bool is_last_tuple(const loads_array &limits)
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
void next_tuple(loads_array &limits)
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

class binconf {
public:
    loads_array loads; // Load of bin 0, bin 1, bin 2, ... 
    std::vector<unsigned int> items; // List of item types (e.g. 2 items of size 5).

    binconf()
    {
	for (int i = 0; i < BINS; i++)
	{
	    loads.push_back(0);
	}
	for (int i = 0; i <= S; i++)
	{
	    items.push_back(0);
	}
    }; 

    // Packs a new item, returns false if the packing creates a bin of size >= R.
    bool pack(int item, int bin)
    {
	assert(bin >= 0 && bin < BINS && item <= S && item >= 1);
	bool admissible = true;
	loads[bin] += item;

	if(loads[bin] >= R) {
	    return false;
	}
	
	items[item]++;
	sort(loads.begin(), loads.end(), std::greater<int>());

	return true;
    };

    // Checks if the optimum can pack the list of items stored into binconf
    // into BINS bins of capacity S along with the next item on input. Uses sparse dynamic programming, as described
    // in the paper.
    bool test_with(int next_item)
	{

        config_queue* cur = new config_queue;
	config_queue* prev = new config_queue;
	
	config_map cur_membership;
	loads_array config;
	loads_array newconf;
	print<DEBUG>("Testing feasibility by an offline optimum.\n");
	

	bool first_item = true;
	
	for (int type=S; type >= 1; type--) {
	    int amount = items[type];
	    if (type == next_item)
	    {
		amount++;
	    }
	    
	    while (amount > 0) {
		if (first_item)
		{
		    for (int bin = 0; bin < BINS; bin++)
		    {
			config.push_back(0);
		    }
		    config[0] = type;
		    cur->push(config);
		    cur_membership[config] = true;
		    first_item = false;
		} else {
		    while (!prev->empty())
		    {
			config = prev->front();
			prev->pop();
			for (int bin = BINS-1; bin >= 0; bin--)
			{
			    if (config[bin] + type > S)
			    {
				break;
			    } else
			    {
				newconf = config;
				newconf[bin] += type;
				sort(newconf.begin(), newconf.end(), std::greater<int>());
				if (cur_membership.find(newconf) == cur_membership.end())
				{
				    cur->push(newconf);
				    cur_membership[newconf] = true;
				}
			    }
			}
		    }
		}
		swap(cur, prev);
		cur_membership.clear();
		// Delete the queue that is no longer needed, currently pointed at by cur.
		delete cur;
		cur = new config_queue;
		amount--;
	    }
	}

	// If no items arrived, it is trivially packable.
	if (first_item == true) {
	    return true;
	}
	// If the queue is still nonempty, a packing is possible.
	else if (!prev->empty())
	{
	    delete prev;
	    return true;
	} else {
	    delete prev;
	    return false;
	}
    }

    bool operator<(const binconf& other) const
	{
	    return ((loads < other.loads) || (items < other.items));
	}

    bool operator==(const binconf& other) const
	{
	    return ((loads == other.loads) && (items == other.items));
	}

    // Validates the consistency of a bin configuration
    // but not the guarantee -- we only check the guarantee
    // in the leaves of the game tree.
    
    bool validate() {
	print<DEBUG>("Validating its bin configuration\n");

	int sumloads = 0, sumtypes = 0;
	for (auto i: loads) {
	    if(i > R || i < 0) return false;
	    sumloads += i;
	}

	for (unsigned int i=1; i<items.size(); i++) {
	    sumtypes += i * items[i];
	}
	
	if(sumloads != sumtypes) return false;

	return true;
    };
};

// Vertex of the game tree.
class vertex {

public:
    /* We do not store pointers to children directly
     * because children are not known at the time of the parent's creation,
     * only their IDs. We store only IDs, at the cost of O(log n) access time.
     */
    std::vector<llu> children; 
    binconf* configuration;
    int nextItem;
    uint64_t id;


    // Whether a vertex is already validated.
    bool validated = false;
    // Whether a vertex is already filled in.
    bool filled_in = false;
    
    vertex(binconf* c_) : configuration(c_) {};
    void fill_types();
    void fill_types(std::vector<int>& initial_items);

    bool validate();
    bool recursive_validate();
    void print_info();
};

// the map of all vertices in the game tree, indexed by ids,
// which are produced by the lower bound generator
std::map<uint64_t, vertex> tree;


void vertex::print_info()
{
    print_array(configuration->loads);
}

template <bool PARAM> void print_vertex(vertex x)
{
    if (PARAM)
    {
	x.print_info();
    }
}


// recursively fills in the loads array; needs to be run
//   after the graph is complete

void vertex::fill_types(std::vector<int>& initial_items)
{
    for (const int item: initial_items)
    {
	print<DEBUG>("Inserting %d into the root item list.\n", item);
	assert(item >= 1 && item <= S);
	(*configuration).items[item] += 1;
    }

    fill_types();
}

void vertex::fill_types() {

    if (filled_in)
    {
	return;
    }
    
    for (uint64_t child_id: children)
    {
	vertex& child = tree.at(child_id);
	std::copy(configuration->items.begin(), configuration->items.end(), child.configuration->items.begin());
	assert(nextItem <= S);
	// print<DEBUG>("Filling next item %d into vertex %llu\n", nextItem, child_id);
	(child.configuration->items[nextItem])++;
	child.fill_types();
    }

    filled_in = true;
}

    /* validate a vertex of the tree */
bool vertex::validate()
{
    print<DEBUG>("Validating vertex %llu\n", id);

    if (validated) { return true; }
    

    if(configuration == NULL) return false;

    if(!configuration->validate())
    {
	return false;
    }

    if(nextItem <= 0 || nextItem > S)
    {
	return false;
    }

    // Check that all possible packings of nextItem into configuration are valid.

    // Check all possible packing positions.
    std::vector<binconf> generated_list;
    for(int i = BINS-1; i>=0; i--)
    {
	binconf next_step(*configuration);
	if(next_step.loads[i] + nextItem >= R)
	{
	    // since the bins are sorted, we can break here
	    break;
	}

	if(i < BINS-1 && next_step.loads[i] == next_step.loads[i-1])
	{
	    continue;
	}

	bool admissible = next_step.pack(nextItem,i);
	assert(admissible == true);

	if (std::find(generated_list.begin(), generated_list.end(), next_step) == generated_list.end())
	{
	    generated_list.push_back(next_step);
	}
    }

    std::sort(generated_list.begin(), generated_list.end());

    // Collect all children
    std::vector<binconf> children_list;

    for (llu child_id: children)
    {
	const vertex& child = tree.at(child_id);
	children_list.push_back(*(child.configuration));
    }

    std::sort(children_list.begin(), children_list.end());

    if (children_list.size() != generated_list.size())
    {
        print<true>("Vertex %" PRIu64 ": children_list %zu, generated_list %zu.\n", id, children_list.size(), generated_list.size());
	assert(children_list.size() == generated_list.size());
    }

    for (int i = 0; i < children_list.size(); i++)
    {
	assert(children_list[i] == generated_list[i]);
    }
    return true;

    /*
    if (!found)
    {
	    print<DEBUG>("One of the valid children of vertex %llu was not found, namely:", id);
	    print_array(next_step.loads);
	    print<DEBUG>("\n");
	    
	    found_all = false;
	    break;
	}
    }
    */
};

bool vertex::recursive_validate()
{
    if (validated) { return true; }

    // We validate consistency of the game tree, but we only test
    // the optimum guarantee on every vertex of outdegree 0, which is sufficient.

    bool selftest = false;

    selftest = validate();
    if (!selftest)
    {
	return false;
    }
    
    if (children.size() == 0)
    {
	selftest = configuration->test_with(nextItem);
	if (!selftest)
	{
	    return false;
	}
    }

	
    for(uint64_t child_id: children)
    {
	vertex& child = tree.at(child_id);
	if(!child.recursive_validate())
	{
	    return false;
	}
    }
    
    validated = true;
    return true;
}
