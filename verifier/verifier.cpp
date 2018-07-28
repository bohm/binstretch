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
    fscanf(fin, "overlap = none;\n");


    // third line -- initial items
    int initial_count = 0, initial = 0;

    if (fscanf(fin, "// %d:", &initial_count) != 1)
    {
	ERROR("Malformed initial item count.\n");
    }

    for (int i = 0; i < initial_count; i++)
    {
	if (fscanf(fin, "%d", &initial) != 1)
	{
		ERROR("Malformed initial item list.\n");
	} else {
	    initial_items.push_back(initial);
	}
    }

    fscanf(fin, "\n");
    
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
	
	fscanf(fin, " %c", &control);

	// vertex descriptor
	if (control == '[')
	{
	    binconf* cc = new binconf(); // current configuration
	    int next;
	    int total = 0;
	    if (fscanf(fin, "label=\"") != 0) {
		ERROR("Failed to parse an edge on line %" PRIu64 "\n", line);
	    }
	    
	    for (int i =0; i < BINS; i++) {
		if (fscanf(fin, "%d", &(cc->loads[i])) != 1)
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
		

		     
		total += cc->loads[i];
	    }
	    
	    if (fscanf(fin, "n: %d\"];\n", &next) != 1) {
		ERROR("Failed to parse an edge on line %" PRIu64 "\n", line);
	    }

	    // Set first vertex as root.
	    if (first_vertex)
	    {
		print<DEBUG>("Setting vertex %" PRIu64 " as root.\n", main_id);
		root_id = main_id;
		first_vertex = false;
	    }

	    vertex cv(cc); // current vertex
	    cv.nextItem = next;
	    cv.id = main_id;
	    print<DEBUG>("Creating vertex %" PRIu64 ": ", main_id);
	    print_vertex<DEBUG>(cv);
	    print<DEBUG>("and next item %d\n", cv.nextItem);
	    tree.insert(make_pair(main_id, cv));
	    line++;
	}
	
	// edge descriptor
	else if (control == '-') {
	    if (fscanf(fin, "> %" PRIu64 "\n", &secondary_id) != 1)
	    {
		ERROR("Failed to parse an edge on line %" PRIu64 "\n", line);
	    }
	    try {
		vertex& relevant = tree.at(main_id); 
		relevant.children.push_back(secondary_id);
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

    print<true>("Finished reading input. Recursively computing loads at each vertex.\n");
    vertex& root = tree.at(root_id);
    root.fill_types(initial_items);
    
    print<true>("Starting tree validation.\n");
    bool result = root.recursive_validate();
    if (result == true)
    {
	fprintf(stdout, "The tree is a correct lower bound with value %d/%d for bin stretching on %d bins starting with vertex:\n", R,S, BINS);
	tree.at(root_id).print_info();
	fprintf(stderr, "\n");
    } else {
	fprintf(stdout, "The tree is not a correct lower bound with value %d/%d on %d bins. Recompile with #define DEBUG 1 to see details.\n", R,S, BINS);
    }
    return 0;
}
