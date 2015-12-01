#define DEBUG 1
#include <cstdio>
#include "verifier.hpp"

using namespace std;

int main(int argc, char **argv)
{
    llu main_id, secondary_id;
    llu line = 1;
    char control;
    FILE *fin;
    if (argc != 2) {
	fprintf(stderr, "Usage: ./verifier file.dot\n");
	fprintf(stderr, "Do not forget to recompile with correct values of R, S and BINS in verifier.hpp.\n");
	fprintf(stderr, "The current values are %d/%d and %d bins.\n", R,S, BINS);
	return -3;
    }
    
    fin = fopen(argv[1], "r");
    if (fin==NULL) {
	ERROR("Unable to open file %s\n", argv[1]);
    }
    int graph_id;
    llu root_id;
    int fscanf_ret;
    char final_brace;

    // first line
    
    fscanf_ret = fscanf(fin, "strict digraph %d {\n", &graph_id);
    if(fscanf_ret != 1) {
	ERROR("Error at line %llu\n", line);
    }
    line++;

    // second line
    fscanf(fin, "overlap = none;\n");
    // data incoming now
    
    while(!feof(fin)) {
	fscanf_ret = fscanf(fin, " %llu", &main_id);
	if(fscanf_ret != 1)
	{
	    fscanf_ret = fscanf(fin,"%c",&final_brace); 
	    if( (fscanf_ret == 1) && (final_brace == '}'))
	    {
		break; // end of input;
	    } else {
		ERROR("Error at line %llu at first number\n", line);
	    }
	}
	
	fscanf(fin, " %c", &control);

	// vertex descriptor
	if(control == '[') {
	    Binconf* cc = new Binconf(); // current configuration
	    int next;
	    int total = 0;
	    if (fscanf(fin, "label=\"") != 0) {
		ERROR("Failed to parse an edge on line %llu\n", line);
	    }
	    
	    for (int i =0; i < BINS; i++) {
		if (fscanf(fin, "%d\\n", &(cc->loads[i])) != 1) {
		    ERROR("Failed to parse an edge on line %llu\n", line);
		}
		total += cc->loads[i];
	    }
	    
	    if (fscanf(fin, "n: %d\"];\n", &next) != 1) {
		ERROR("Failed to parse an edge on line %llu\n", line);
	    }

	    // If configuration is (0,0,0,...,0), set it as root.
	    if (total == 0)
	    {
		DEBUG_PRINT("Setting vertex %llu as root.\n", main_id);
		root_id = main_id;
	    }

	    Vertex cv(cc); // current vertex
	    cv.nextItem = next;
	    cv.id = main_id;
	    DEBUG_PRINT("Creating vertex %llu: ", main_id);
	    DEBUG_PRINT_VERTEX(cv);
	    DEBUG_PRINT("and next item %d\n", cv.nextItem);
	    tree.insert(make_pair(main_id, cv));
	    line++;
	}
	
	// edge descriptor
	else if (control == '-') {
	    if (fscanf(fin, "> %llu\n", &secondary_id) != 1)
	    {
		ERROR("Failed to parse an edge on line %llu\n", line);
	    }
	    try {
		Vertex& relevant = tree.at(main_id); 
		relevant.children.push_back(secondary_id);
		DEBUG_PRINT("Adding edge from %llu to %llu\n", main_id, secondary_id);
	    } catch (exception &e) {
		ERROR("Failed to find a relevant vertex for the vertex %llu\n", main_id);
	    }
	    
	    line++;
	}
	// undefined descriptor
	else {
	    ERROR("Character read was %c\n", control);
	}
    }
    fclose(fin);

    DEBUG_PRINT("Recursively computing loads at each vertex.\n");
    Vertex& root = tree.at(root_id);
    root.fill_types();
    
    DEBUG_PRINT("Starting tree validation.\n");
    bool result = root.recursive_validate();
    if (result == true)
    {
	fprintf(stdout, "The tree is a correct lower bound with value %d/%d for bin stretching on %d bins.\n", R,S, BINS);
    } else {
	fprintf(stdout, "The tree is not a correct lower bound with value %d/%d on %d bins. Recompile with #define DEBUG 1 to see details.\n", R,S, BINS);
    }
    return 0;
}
