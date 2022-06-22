#ifndef _FILETOOLS_HPP
#define _FILETOOLS_HPP 1

#include <ctime>
#include "common.hpp"

void folder_checks()
{
    if (!fs::is_directory(LOG_DIR))
    {
	fprintf(stderr, "The log directory is not found, terminating.\n");
	exit(-1);
    }

    if (!fs::is_directory(OUTPUT_DIR))
    {
	fprintf(stderr, "The output directory is not found, terminating.\n");
	exit(-1);
    }
}

std::string filename_timestamp(const std::tm* time)
{
    char bufstr[100];
    strftime(bufstr, sizeof(bufstr), "--%Y-%m-%d--%H-%M-%S", time);
    std::string ret(bufstr);
    return ret;
}

std::string filename_binstamp()
{
    std::stringstream ss;
    ss << BINS;
    ss << "bins-";
    ss << R;
    ss << "-";
    ss << S;
    return ss.str();
}

// Builds a filename for the top of the tree, which is output to logs/
// for analysis/logging purposes.
// We pass the timestamp so that this file and other log files have the same stamp.
std::string build_treetop_filename(std::tm* timestamp)
{
    	std::string binstamp = filename_binstamp();
	std::string timestamp_str = filename_timestamp(timestamp);
	std::string log_path = LOG_DIR + "/" + binstamp + timestamp_str + "-treetop.log";

	return log_path;

}

std::string build_output_filename(std::tm* timestamp)
{
    	std::string binstamp = filename_binstamp();
	std::string timestamp_str = filename_timestamp(timestamp);
	std::string gen_path = OUTPUT_DIR + "/" + binstamp + timestamp_str + "-tree.gen";
	return gen_path;
}


// A quick and dirty load binconf for a startup of a computation.
// In the grand scheme of things, this should not be needed, but to create
// a full loadfile takes too long.


std::array<bin_int, BINS+1> load_segment_with_loads(FILE* fin)
{
    std::array<bin_int, BINS+1> ret = {};
    
    assert(fscanf(fin, "[") == 0);
    for (int i = 1; i <= BINS; i++)
    {
	if (fscanf(fin, "%" SCNd16, &(ret[i])) != 1)
	{
	    ERROR("Could not scan the %d-th item from the loads segment.\n", i);
	}

    }
    assert(fscanf(fin, "]") == 0);
    return ret;
}

std::array<bin_int, S+1> load_segment_with_items(FILE* fin)
{
    std::array<bin_int, S+1> ret = {};
    
    assert(fscanf(fin, " (") == 0);
    for (int j = 1; j <= S; j++)
    {
	if (fscanf(fin, "%" SCNd16, &(ret[j])) != 1)
	{
	    ERROR("Could not scan the %d-th item from the items segment.\n", j);
	}
    }
 
    assert(fscanf(fin, ")") == 0);
    return ret;
}

bin_int load_last_item_segment(FILE *fin)
{
    bin_int last_item = 0;
    if(fscanf(fin, "%" SCNd16, &last_item) != 1)
    {
	    ERROR("Could not scan the last item field from the input file.\n");
    }
    return last_item;
}

binconf loadbinconf(const char* filename)
{
    FILE* fin = fopen(filename, "r");
    if (fin == NULL)
    {
	ERROR("Unable to open file %s\n", filename);
    }

    std::array<bin_int, BINS+1> loads = load_segment_with_loads(fin);
    std::array<bin_int, S+1> items = load_segment_with_items(fin);
    bin_int last_item = load_last_item_segment(fin);
    // fprintf(stderr, "Loadbinconf: Loaded last item %d.\n", (int) last_item);

    fclose(fin);

    binconf retbc(loads, items, last_item);
    return retbc;
}


#endif // _FILETOOLS_HPP
