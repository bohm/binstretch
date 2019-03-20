#ifndef _FILESTORAGE_HPP
#define _FILESTORAGE_HPP 1
// Loading and storing saplings from/to files.
#include <filesystem>
#include <iostream>
#include "dag.hpp"

namespace fs = std::filesystem;

void folder_check()
{
}

// loads a sapling and creates an adversary vertex with it
// assumes zobrist hashes are initialized properly

adversary_vertex* load_sapling_file(FILE* stream)
{
    
    binconf bc;

    bc.loads[0] = 0;
    bool first = true;
    for (int i = 1; i <= BINS; i++)
    {
	if (first)
	{
	    first = false;
	    fscanf(stream, "%hd", &(bc.loads[i]));
	} else {
	    fscanf(stream, "-%hd", &(bc.loads[i]));
	}
    }

    bc.items[0] = 0;
    first = true;
    for (int i = 1; i <= S; i++)
    {
	if (first)
	{
	    first = false;
	    fscanf(stream, " (%hd", &(bc.items[i]));
	} else {
	    if(i % 11 == 1)
	    {
		fscanf(stream, "|%hd", &(bc.items[i]));
	    } else
	    {
		fscanf(stream, ",%hd", &(bc.items[i]));
	    }
	}
    }

    bc.hash_loads_init();
    // sanity check
    print<true>("Loaded: ");
    print_binconf<true>(&bc);
    // sapling creates a copy of bc, which is only a local copy
    adversary_vertex* sapling = new adversary_vertex(&bc, 0, 1);
    return sapling;
}

// loads a sapling
fs::path loaded_file;
adversary_vertex* load_sapling()
{
    adversary_vertex *ret = NULL;
    if (fs::begin(fs::directory_iterator("./sap/")) != fs::end(fs::directory_iterator("./sap/")))
    {
	fs::directory_entry ent = *(fs::begin(fs::directory_iterator("./sap/")));
	loaded_file = ent.path();
	FILE* fstream = fopen(ent.path().c_str(), "r");
	ret = load_sapling_file(fstream);
	fclose(fstream);
    }
    return ret;
}

int remaining_sapling_files()
{
    int count = 0;
    auto it = fs::begin(fs::directory_iterator("./sap/"));
    while (it != fs::end(fs::directory_iterator("./sap/")))
    {
	count++;
	it++;
    }
    return count;
}

void write_solution(adversary_vertex *sapling, int solution, int monotonicity)
{
    std::stringstream sst; 
    sst << "./sol/" << loaded_file.filename().replace_extension(".sol").string();
    fprintf(stderr, "Opening file %s for writing.\n", sst.str().c_str());
    FILE* sol = fopen(sst.str().c_str(), "w");
    assert(sol != NULL);
    print_binconf_stream(sol, sapling->bc);

    assert(solution == 0 || solution == 1);
    if( solution == 0)
    {
	fprintf(sol, "Leads to a lower bound with monotonicity %d.\n", monotonicity);
    } else {
	fprintf(sol, "Can be won by Algorithm with final monotonicity %d.\n", monotonicity);
    }

    fclose(sol);
    fs::remove(loaded_file);
}


/
// populates the directory with saplings from sequencing 
void write_sapling_queue()
{
    std::stack<sapling> sapling_copy = sapling_stack;
    assert(fs::is_directory("./sap/"));
    int counter = 0;
    char filename[20] = {0};
    while(!sapling_copy.empty())
    {
	adversary_vertex* sapling = sapling_copy.top();
	sapling_copy.pop();
	sprintf(filename, "./sap/sapling%d.txt", counter);
	printf("Filename to write: %s.\n", filename);
	FILE* sap = fopen(filename, "w");
	assert(sap != NULL);
	print_binconf_stream(sap, sapling->bc);
	fclose(sap);
	counter++;
    }
}

#endif
