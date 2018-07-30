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

// Largest id seen so far in graph. If we want to create auxiliary vertices, we increment this.
// We do not assume the input graph has continuous IDs.
uint64_t largest_id = 0;

uint64_t heuristics_counter = 0;

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
    
    for(unsigned int bin = 0; bin < BINS; bin++)
    {
	limits.push_back(0);
    }
}

// Checks if the array limits is the tuple (S,S,...,S).
bool is_last_tuple(const loads_array &limits)
{
    for(unsigned int bin = 0; bin < BINS; bin ++)
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
    // We explicitly convert BINS-1 to (int) so that we allow bin--; to end correctly.
    for (int bin = (int) (BINS-1); bin >=0; bin--)
    {
	if( limits[bin] <  S)
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
    std::vector<unsigned int> loads; // Load of bin 0, bin 1, bin 2, ... 
    std::vector<unsigned int> items; // List of item types (e.g. 2 items of size 5).

    binconf()
    {
	loads.clear();
	items.clear();
	for (unsigned int i = 0; i < BINS; i++)
	{
	    loads.push_back(0);
	}
	for (unsigned int i = 0; i <= S; i++)
	{
	    items.push_back(0);
	}
    }; 

    // copying a previous bin configuration
    binconf(const binconf &other)
	{
	    loads.clear();
	    items.clear();
	    assert(other.loads.size() == BINS && other.items.size() == S+1);

	    for (unsigned int i = 0; i < BINS; i++)
	    {
		loads.push_back(other.loads[i]);
	    }
	    
	    assert(other.items.size() == S+1);
	    
	    for (unsigned int i = 0; i <= S; i++)
	    {
		items.push_back(other.items[i]);
	    }
	}

    // Prints the bin configuration.
    template <bool MODE> void print_binconf()
	{
	    if (MODE)
	    {
		print_array(loads);
		print<true>(" ");
		assert(items.size() == S+1);
		print<true>("[");
		for (unsigned int i = 1; i <= S; i++)
		{
		    if (i > 1)
		    {
			print<true>(", ");
		    }

		    print<true>("%d", items[i]);
		}
		print<true>("] ");
	    }
	}
    
    // Packs a new item, returns false if the packing creates a bin of size >= R.
    bool pack(unsigned int item, unsigned int bin)
    {
	assert(bin >= 0 && bin < BINS && item <= S && item >= 1);

	if(loads[bin] + item >= R) {
	    return false;
	}
	loads[bin] += item;
	items[item]++;
	sort(loads.begin(), loads.end(), std::greater<int>());

	return true;
    };

    
    // Checks if the optimum can pack the list of items stored into binconf
    // into BINS bins of capacity S along with the next item on input. Uses sparse dynamic programming, as described
    // in the paper.
    bool test_with(unsigned int next_item)
	{

        config_queue* cur = new config_queue;
	config_queue* prev = new config_queue;
	
	config_map cur_membership;
	loads_array config;
	loads_array newconf;
	bool first_item = true;
	
	for (unsigned int type=S; type >= 1; type--)
	{
	    int amount = items[type];
	    if (type == next_item)
	    {
		amount++;
	    }
	    
	    while (amount > 0)
	    {
		if (first_item)
		{
		    for (unsigned int bin = 0; bin < BINS; bin++)
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
			for (int bin = (int) (BINS-1); bin >= 0; bin--)
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
	    delete prev;
	    delete cur;
	    return true;
	}
	// If the queue is still nonempty, a packing is possible.
	else if (!prev->empty())
	{
	    delete prev;
	    delete cur;
	    return true;
	} else {
	    delete prev;
	    delete cur;
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

	assert(loads.size() == BINS && items.size() == S+1);
	
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

const int LARGE_ITEM = 1;
const int FIVE_NINE = 2;

// Vertex of the game tree.
class vertex {

public:
    /* We do not store pointers to children directly
     * because children are not known at the time of the parent's creation,
     * only their IDs. We store only IDs, at the cost of O(log n) access time.
     */
    std::vector<llu> children; 
    binconf configuration;
    int nextItem = 0;
    uint64_t id = 0;

    bool visited = false;
    // Whether a vertex is already validated.
    bool validated = false;
    // Whether a vertex is already filled in.
    bool filled_in = false;

    // Whether a vertex was read as a heuristic.
    // Amount means how many items should be sent when the
    // vertex is expanded.
    // Possible values:
    // 0 == no heuristic
    // 1 == LARGE_ITEM
    // 2 == FIVE_NINE
    
    int heuristic_type = 0;
    int heuristic_item_size = 0;
    int heuristic_item_amount = 0;

    // When id is read from the input file, we construct the vertex this way.
    vertex(const binconf& c_, uint64_t id_, int nextItem_) : configuration(c_), nextItem(nextItem_), id(id_) {};

    // When id is absent, we construct it this way.
    vertex(const binconf& c_, int nextItem_) : configuration(c_), nextItem(nextItem_)
	{
	    id = ++largest_id;
	}

    vertex(const binconf& c_, uint64_t id_, int htype, int hitem_size, int hitem_amount) :
	configuration(c_), nextItem(0),  id(id_), heuristic_type(htype), heuristic_item_size(hitem_size),heuristic_item_amount(hitem_amount) {}


    void fill_types();
    void fill_types(std::vector<int>& initial_items);

    // Adding an edge is just pushing into the children list.
    void add_edge(uint64_t next_adversarial_id)
	{
	    children.push_back(next_adversarial_id);
	}

    bool validate();
    bool recursive_validate();
    void print_info();
    void expand_heuristics(int h_type, int h_item_size, int h_item_count);
    void expand_heuristics();
};

// The map of all vertices in the game tree, indexed by ids,
// which are produced by the lower bound generator.
std::map<uint64_t, vertex*> tree;


// Traverse the whole graph and check if a vertex matches the bin configuration.
std::pair<bool, uint64_t> slowfind_id(const binconf& bc)
{
    bool found = false;
    uint64_t found_id = 0;

    for (auto& [vid, vertpoint] : tree)
    {
	if (vertpoint->configuration == bc)
	{
	    found = true;
	    found_id = vid;
	    break;
	}
    }
    
    return std::pair(found, found_id);
}

void vertex::print_info()
{
    print_array(configuration.loads);
}

template <bool PARAM> void print_vertex(vertex* x)
{
    if (PARAM)
    {
	x->print_info();
    }
}

// Constructs a new vertex from a bin configuration, heuristic type
// and a queue of incoming items.
// Used when expanding heuristics only.
void vertex::expand_heuristics(int h_type, int h_item_size, int h_item_count)
{

    // Basic sanity checks.
    assert(h_type != 0 && h_item_count >= 0 && h_item_size >= 1 && h_item_size <= (int) S);

    // Assert that we are expanding a vertex with no nextItem
    // and no children.
    assert(nextItem == 0 && children.size() == 0);

    if (h_type == FIVE_NINE)
    {
	// The five/nine heuristic sends h_item_count objects of size 5.
	// If at any time any bin reaches load of 10, it starts sending
	// 9s.

	// Also, this is the only case where h_item_count can be zero.
	
	bool bin_at_least_ten = false;
	for (unsigned int bin = 0; bin < BINS; bin++)
	{
	    if (configuration.loads[bin] >= 10)
	    {
		bin_at_least_ten = true;
	    }
	}

	// If such a bin exists, we send a sequence of BINS times 9.
	// We implement this by switching to large item heuristic expansion.
	if (bin_at_least_ten)
	{
	    expand_heuristics(LARGE_ITEM, 9, BINS);
	    return;
	}

	if (h_item_count == 0)
	{

	    // Once the five/nine heuristic sends enough items of size 5,
	    // it starts sending items of size 14. After enough of them,
	    // one bin will be of size >= 19.
	    // We again implement this by switching to the large item heuristic.

	    // Note that we send more items than technically feasible; still,
	    // since the heuristic is supposed to be true, we will reach 19
	    // while the configuration is still valid w.r.t. the adversarial guarantee.

	    expand_heuristics(LARGE_ITEM, 14, BINS);
	    return;
	}
    }
	
    nextItem = h_item_size;
    // Heuristic item count cannot be zero at this point.
    assert(h_item_count > 0);
    
    for (unsigned int bin = 0; bin < BINS; bin++)
    {

	// Skip if two sequential bins have the same load.
	if (bin >= 1 && configuration.loads[bin] == configuration.loads[bin-1])
	{
	    continue;
	}
	
	if (configuration.loads[bin] + nextItem < R)
	{
	    binconf newbc(configuration);
	    newbc.pack(nextItem, bin);

	    // We add a new vertex when expanding even though the vertex
	    // may already be present in the tree. This is slightly suboptimal,
	    // but it is also quite simple.

	    // temporarily set nextItem to 0
	    vertex* new_ver = new vertex(newbc, 0);

	    tree.insert(std::pair(new_ver->id, new_ver));
		
	    add_edge(new_ver->id);
		
	    print<DEBUG>("Created auxiliary vertex %" PRIu64 " out from heur. vertex %" PRIu64 " when sending %d.\n", new_ver->id, id, nextItem);

	    new_ver->expand_heuristics(h_type, h_item_size, h_item_count-1);
	}
    }
}

void vertex::expand_heuristics()
{
    assert(heuristic_type != 0 && nextItem == 0);
    expand_heuristics(heuristic_type, heuristic_item_size, heuristic_item_amount);
    heuristic_type = heuristic_item_size = heuristic_item_amount = 0;
    assert(nextItem != 0);
}

// Marks all vertices as unvisited.
void clear_visits()
{
    for (auto& [vid, vertpointer] : tree)
    {
	vertpointer->visited = false;
    }
}

// Traverses the whole graph using DFS, expands all heuristic vertices.
uint64_t vertices_expanded = 0;

void expand_all_vertices(vertex* v)
{
    if (v->visited)
    {
	return;
    }

    v->visited = true;
    
    if (v->heuristic_type != 0)
    {
	print<DEBUG>("Expanding heuristic vertex %" PRIu64 ": ", v->id );
	v->configuration.print_binconf<DEBUG>();
	print<DEBUG>("\n");
	v->expand_heuristics();
	vertices_expanded++;
	if (vertices_expanded % 1000 == 0)
	{
	    print<DEBUG>("Expanded %" PRIu64 " vertices so far.\n", vertices_expanded);
	}
    } else
    {
	print<DEBUG>("Vertex %" PRIu64 ", not heuristic.\n", v->id );
	for(uint64_t child: v->children)
	{
	    expand_all_vertices(tree.at(child)); 
	}
    }
}

// recursively fills in the loads array; needs to be run
//   after the graph is complete

void vertex::fill_types(std::vector<int>& initial_items)
{
    for (const int item: initial_items)
    {
	print<DEBUG>("Inserting %d into the root item list.\n", item);
	assert(item >= 1 && item <= (int) S);
	configuration.items[item] += 1;
    }

    fill_types();
}

void vertex::fill_types()
{

    if (filled_in)
    {
	return;
    }

    // At this point, its loads should equal its items.
    assert(configuration.validate() == true);

    // Ignore heuristic types; they will be expanded in the next phase.
    if (heuristic_type != 0)
    {
	assert(children.size() == 0);
	// return;
    } else
    {

	for (uint64_t child_id: children)
	{
	    print<DEBUG>("Filling next item %d into vertex %llu\n", nextItem, child_id);
	    vertex* child = tree.at(child_id);
	    std::copy(configuration.items.begin(), configuration.items.end(), child->configuration.items.begin());
	    assert(nextItem <= (int) S);
	    (child->configuration.items[nextItem])++;
	    child->fill_types();
	}
    }

    // This vertex is now complete, so we can add it into the map of
    // bin configurations -> vertex ids.
    filled_in = true;
}


// Validate a vertex of the tree.
bool vertex::validate()
{
    print<DEBUG>("Validating vertex %llu\n", id);

    if (validated) { return true; }
    

    if(!configuration.validate())
    {
	return false;
    }

    if(nextItem <= 0 || nextItem > (int) S)
    {
	return false;
    }

    // Check that all possible packings of nextItem into configuration are valid.

    // Check all possible packing positions.
    std::vector<binconf> generated_list;
    for(int i = BINS-1; i>=0; i--)
    {
	binconf next_step(configuration);
	if (next_step.loads[i] + nextItem >= R)
	{
	    // since the bins are sorted, we can break here
	    break;
	}

	if (i < (int) BINS-1 && next_step.loads[i] == next_step.loads[i+1])
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

    for (uint64_t child_id: children)
    {
	const vertex* child = tree.at(child_id);
	children_list.push_back(child->configuration);
    }

    std::sort(children_list.begin(), children_list.end());

    if (children_list.size() != generated_list.size())
    {
        print<true>("Vertex %" PRIu64 ": children_list %zu, generated_list %zu.\n", id, children_list.size(), generated_list.size());
	print<true>("List of children as stored in vertex:\n");
	for (unsigned int i = 0; i < children_list.size(); i++)
	{
	    print<true>("ID %" PRIu64 ": ", children[i]);
	    children_list[i].print_binconf<true>();
	    print<true>("\n");

	}

	print<true>("\n List of children as computed:\n");

	for (unsigned int i = 0; i < generated_list.size(); i++)
	{
	    generated_list[i].print_binconf<true>();
	    print<true>("\n");
	}

	assert(children_list.size() == generated_list.size());
    }

    for (unsigned int j = 0; j < children_list.size(); j++)
    {
	if (!(children_list[j] == generated_list[j]))
	{
	    print<true>("Mismatch in %u-th item of children's list and the generated list of vertex % " PRIu64 ".\n", j, id);
	    print<true>("Contents of vertex:\n");
	    configuration.print_binconf<true>();

	    print<true>("List of children as stored in vertex:\n");
	    for (unsigned int i = 0; i < children_list.size(); i++)
	    {
		print<true>("ID %" PRIu64 ":\n", children[i]);
		children_list[i].print_binconf<true>();
	    }

	    print<true>("\n List of children as computed:\n");
	    
	    for (unsigned int i = 0; i < generated_list.size(); i++)
	    {
		generated_list[i].print_binconf<true>();
	    }

	    assert(children_list[j] == generated_list[j]);
	}
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
	print<DEBUG>("Testing feasibility of leaf vertex %" PRIu64" with configuration: \n", id);
        configuration.print_binconf<DEBUG>();
	print<DEBUG>("\n");
	selftest = configuration.test_with(nextItem);
	if (!selftest)
	{
	    return false;
	}
    }

	
    for(uint64_t child_id: children)
    {
	vertex* child = tree.at(child_id);
	if(!(child->recursive_validate()))
	{
	    return false;
	}
    }
    
    validated = true;
    return true;
}
