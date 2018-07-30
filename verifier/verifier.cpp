#include "verifier.hpp"

using namespace std;

int main(int argc, char **argv)
{
    char control, final_brace, separator;
    FILE *fin;
    char graph_name[255];
    int fscanf_ret;
    uint64_t main_id, secondary_id, line = 1;
    bool first_vertex = true;
    std::vector<int> initial_items;

    if (argc != 5)
    {
	fprintf(stderr, "Usage: ./verifier file.dot R S BINS\n");
	fprintf(stderr, "R = stretched bin capacity, S = optimal bin capacity.\n");
	return -3;
    }

    fin = fopen(argv[1], "r");
    if (fin == NULL)
    {
	ERROR("Unable to open file %s\n", argv[1]);
    }

    sscanf(argv[2], "%u", &R); assert(R >= 1 && S <= 255);
    sscanf(argv[3], "%u", &S); assert(S >= 1 && S <= 255);
    sscanf(argv[4], "%u", &BINS); assert(BINS >= 3 && BINS <= 255);

    // first line
    
    fscanf_ret = fscanf(fin, "strict digraph %s {\n", graph_name);
    if (fscanf_ret != 1)
    {
	ERROR("Error at line %" PRIu64 "\n", line);
    }
    line++;

    // second line
    if (fscanf(fin, "overlap = none;\n") != 0)
    {
	abort();
    }


    // third line -- initial items
    int initial_count = 0, initial = 0;

    if (fscanf(fin, "// %d:", &initial_count) != 1)
    {
	ERROR("Malformed initial item count.\n");
    }

    for (int i = 0; i < initial_count; i++)
    {
	if (fscanf(fin, " %d", &initial) != 1)
	{
		ERROR("Malformed initial item list.\n");
	} else {
	    initial_items.push_back(initial);
	}
    }

    if(fscanf(fin, "\n") != 0)
    {
	abort();
    }
    
    while (!feof(fin))
    {
	fscanf_ret = fscanf(fin, " %" PRIu64 "", &main_id);
	if (fscanf_ret != 1)
	{
	    fscanf_ret = fscanf(fin,"%c",&final_brace); 
	    if ((fscanf_ret == 1) && (final_brace == '}'))
	    {
		break; // end of input;
	    } else {
		ERROR("Error at line %" PRIu64 " at first number\n", line);
	    }
	}

	largest_id = std::max(main_id, largest_id);
	if (fscanf(fin, " %c", &control) != 1)
	{
	    abort();
	}
	
	
	// vertex descriptor
	if (control == '[')
	{
	    binconf cc; // current configuration
	    // print<true>("Sanity check: ");
	    // cc.print_binconf();
	    int next = 0;
	    int htype = 0;
	    int hitem_amount = 0;
	    int hitem_size = 0;
	       
	    if (fscanf(fin, "label=\"") != 0) {
		ERROR("Failed to parse an edge on line %" PRIu64 "\n", line);
	    }
	    
	    for (unsigned int i =0; i < BINS; i++)
	    {
		if (fscanf(fin, "%d", &(cc.loads[i])) != 1)
		{
		    ERROR("Failed to parse a vertex on line %" PRIu64 "\n", line);
		}

		// two allowed separators -- "\n" and " ".
		if (fscanf(fin, "%c", &separator) != 1)
		{
		    ERROR("Failed to scan a separator on line %" PRIu64 "\n", line);
		}

		if (separator == '\\' )
		{
		    if (fscanf(fin, "%c", &separator) != 1)
		    {
			ERROR("Failed to scan a second character of the separator on line %" PRIu64 ".\n", line);
		    }

		    if (separator != 'n')
		    {
			ERROR("Backslash on line %" PRIu64 " is not followed by 'n'.\n", line);
		
		    }
		} else if (separator != ' ')
		{
		    ERROR("Separator on line %" PRIu64 " is not one of the valid ones.\n", line);
		} 
	    }

	    char next_control;
	    if(fscanf(fin, "%c", &next_control) != 1)
	    {
		ERROR("Failed to parse next/heuristic character on line %" PRIu64 "\n", line);
	    }
		    
	    if (next_control == 'n')
	    {
		if (fscanf(fin, ": %d\"];\n", &next) != 1) {
		    ERROR("Failed to parse next item on line %" PRIu64 "\n", line);
		}
	    } else if (next_control == 'h')
	    {
		char heur_control;
		heuristics_counter++;
		
		if(fscanf(fin, ":%c", &heur_control) != 1)
		{
		    ERROR("Failed to recognize heuristic on line %" PRIu64 "\n", line);
		}
		
		if (heur_control == '(')
		{
		    htype = LARGE_ITEM;
		    if (fscanf(fin, "%d,%d)\"];\n", &hitem_size, &hitem_amount) != 2)
		    {
			ERROR("Failed to complete large item heuristic on line %" PRIu64 "\n", line);
		    }
	    
		}
		else if (heur_control == 'F')
		{
		    htype = FIVE_NINE;
		    hitem_size = 5;
		    if (fscanf(fin, "N (%d)\"];\n", &hitem_amount) != 1)
		    {
			ERROR("Failed to parse the heuristic on line %" PRIu64 "\n", line);
		    }
	    
		} else {
		    // unknown heuristic
		    ERROR("Failed to recognize heuristic on line %" PRIu64 "\n", line);
		}

	    }
	    else
	    {
		ERROR("Unknown character %c on line %" PRIu64 " where we expected 'n' or 'h'.\n", next_control, line);
	    }

	    // Set first vertex as root.
	    if (first_vertex)
	    {
		print<DEBUG>("Setting vertex %" PRIu64 " as root.\n", main_id);
		print<DEBUG>("Its contents:");
		cc.print_binconf<DEBUG>();
		root_id = main_id;
		first_vertex = false;
	    }

	    // We create the vertex in heap so that the tree map 
	    // serves as a list of all vertices as well, and vertices inside can be edited.
	    
	    vertex* cv;

	    if (htype == 0)
	    {
		cv = new vertex(cc, main_id, next);
		print<DEBUG>("Creating vertex %" PRIu64 ": ", main_id);
	    } else {
		cv = new vertex(cc, main_id, htype, hitem_size, hitem_amount);
		print<DEBUG>("Creating new heuristic vertex %" PRIu64 ": ", main_id);
	    }
	    
	    print_vertex<DEBUG>(cv);
	    print<DEBUG>("and next item %d\n", cv->nextItem);

	    tree.insert(std::pair(main_id, cv));
	    line++;
	}
	
	// edge descriptor
	else if (control == '-') {
	    if (fscanf(fin, "> %" PRIu64 "\n", &secondary_id) != 1)
	    {
		ERROR("Failed to parse an edge on line %" PRIu64 "\n", line);
	    }
	    try {
		vertex* relevant = tree.at(main_id); 
		relevant->children.push_back(secondary_id);
		print<DEBUG>("Adding edge from %" PRIu64 " to %" PRIu64 "\n", main_id, secondary_id);
	    } catch (exception &e) {
		ERROR("Failed to find a relevant vertex for the vertex %" PRIu64 "\n", main_id);
	    }
	    
	    line++;
	}
	// undefined descriptor
	else  
	{
	    ERROR("Character read was %c\n", control);
	}
    }
    fclose(fin);
    vertex* root = tree.at(root_id);

    print<true>("Finished reading input, tree has %zu vertices, out of which %d are heuristic nodes.\n Recursively computing item count.\n", tree.size(), heuristics_counter);
    root->fill_types(initial_items);

    print<true>("Expanding heuristic strategies to full strategies.\n");
    clear_visits();
    expand_all_vertices(root);
    print<true>("Expanded %" PRIu64 " vertices.\n", vertices_expanded);

    print<true>("Starting tree validation.\n");
    bool result = root->recursive_validate();
    if (result == true)
    {
	fprintf(stdout, "The tree is a correct lower bound with value %d/%d for bin stretching on %d bins starting with:\n", R,S, BINS);
	tree.at(root_id)->configuration.print_binconf<true>();
	fprintf(stderr, "\n");
    } else {
	if(DEBUG)
	{
	    fprintf(stdout, "The tree is not a correct lower bound with value %d/%d on %d bins.\n", R,S, BINS);
	}
	else 
	{
	    fprintf(stdout, "The tree is not a correct lower bound with value %d/%d on %d bins. Recompile with const DEBUG = true; to see more details.\n", R,S, BINS);
	}
    }

    // cleanup

    for (auto [id, point] : tree)
    {
	delete point;
    }
    tree.clear();
    return 0;
}
