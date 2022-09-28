#ifndef _FILETOOLS_HPP
#define _FILETOOLS_HPP 1

#include <ctime>
#include <iostream>
#include <sstream>

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


std::array<bin_int, BINS+1> load_segment_with_loads(std::stringstream& str_s)
{
    std::array<bin_int, BINS+1> ret = {};

    char c = 0;
    bin_int load = -1;
    str_s.get(c);
    assert(c == '[');
    for (int i = 1; i <= BINS; i++)
    {
	str_s >> load;
	if (load < 0 || load >= R)
	{
	    ERROR("The %d-th load from the loads list is out of bounds.", i);
	}
	ret[i] = load;
	load = -1;
    }

    str_s.get(c);
    assert(c == ']');
    return ret;
}

std::array<bin_int, S+1> load_segment_with_items(std::stringstream& str_s)
{
    std::array<bin_int, S+1> ret = {};

    char c = 0;
    bin_int item_size = -1;
    str_s.get(c);
    assert(c == ' ');
    str_s.get(c);

    assert(c == '(');
   
    for (int j = 1; j <= S; j++)
    {
	str_s >> item_size;
	if (item_size < 0 || item_size > BINS*S)
	{
	    ERROR("The %d-th item from the items segment is out of bounds.\n", j);
	}

	ret[j] = item_size;
	item_size = -1;
    }
    
    str_s.get(c);
    assert(c == ')');

    return ret;
}

bin_int load_last_item_segment(std::stringstream& str_s)
{
    bin_int last_item = -1;
    str_s >> last_item;

    if (last_item < 0 || last_item > BINS*S)
    {
	ERROR("Could not scan the last item field from the input file.\n");
    }
    return last_item;
}

binconf loadbinconf(std::stringstream& str_s)
{
    std::array<bin_int, BINS+1> loads = load_segment_with_loads(str_s);
    std::array<bin_int, S+1> items = load_segment_with_items(str_s);
    bin_int last_item = load_last_item_segment(str_s);

    binconf retbc(loads, items, last_item);
    retbc.hashinit();
    return retbc;
   
}

binconf loadbinconf_singlefile(const char* filename)
{
    FILE* fin = fopen(filename, "r");
    if (fin == NULL)
    {
	ERROR("Unable to open file %s\n", filename);
    }

    char linebuf[1024];
    char* retptr = fgets(linebuf, 1024, fin);
    if (retptr == nullptr)
    {
	ERROR("File %s found, but a line could not be loaded.\n", filename);
    }
    
    fclose(fin);
    std::string line(linebuf);
    std::stringstream str_s(line);

    return loadbinconf(str_s);
}

#endif // _FILETOOLS_HPP
