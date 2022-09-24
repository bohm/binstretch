#ifndef _ASSUMPTIONS_HPP
#define _ASSUMPTIONS_HPP 1

#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "common.hpp"
#include "constants.hpp"
bool USING_ASSUMPTIONS = false;
char ASSUMPTIONS_FILENAME[256];

class assumption_unit
{
public:
    binconf bc;
    victory win;
};
    
class assumptions
{

    std::vector<assumption_unit> assume_arr;
    std::map<uint64_t, int> position_map; // Currently used only for debugging purposes.
    std::map<uint64_t, victory> victory_map;
    
public:
    // Check that all hashes of the array appear once.
    void check_uniqueness() const
	{
	    std::vector<uint64_t> hashes;
	    for (auto& assumption: assume_arr)
	    {
		hashes.push_back(assumption.bc.hash_with_last());
	    }

	    std::sort(hashes.begin(), hashes.end());
	    const auto duplicate = std::adjacent_find(hashes.begin(), hashes.end());
	    
	    if (duplicate != hashes.end())
	    {
		uint64_t duplicate_hash = *duplicate;
		int position = position_map.at(duplicate_hash);
		fprintf(stderr, "Duplicate entry in assumption list: ");
		print_binconf_stream(stderr, assume_arr[position].bc);
		exit(-1);
	    }
	}


    void load_file(const char *filename)
	{
	    FILE* assumefin = fopen(filename, "r");
	    if (assumefin == NULL)
	    {
		ERROR("Unable to open file %s\n", filename);
	    }

	    while(!feof(assumefin))
	    {
		char linebuf[1024];
		if (fgets(linebuf, 1024, assumefin) == nullptr)
		{
		    break;
		}
		std::string line(linebuf);
		std::stringstream str_s(line);
		
		// Using filetools functions to load the bin configuration part.
		std::array<bin_int, BINS+1> loads = load_segment_with_loads(str_s);
		std::array<bin_int, S+1> items = load_segment_with_items(str_s);
		bin_int last_item = load_last_item_segment(str_s);
		binconf curbc(loads, items, last_item);
		curbc.hashinit();

		std::string rest;
		std::getline(str_s, rest);
		// Load the assumption
		char textual_assumption[30];
		if(sscanf(rest.c_str(), " assumption: %s\n", textual_assumption) != 1)
		{
		    ERROR("Assumption %d failed to load.\n", assume_arr.size());
		}

		curbc.consistency_check();
		
		assumption_unit asu; asu.bc = curbc;
		if (strcmp(textual_assumption, "adv") == 0)
		{
		    asu.win = victory::adv;
		} else if (strcmp(textual_assumption, "alg") == 0)
		{
		    asu.win = victory::alg;
		} else
		{
		    ERROR("Assumption %d failed to load.\n", assume_arr.size());
		}
		
		assume_arr.push_back(asu);
		int position = assume_arr.size()-1;
		victory_map.insert({asu.bc.hash_with_last(), asu.win});
		position_map.insert({asu.bc.hash_with_last(), position});
		// assert(fscanf(assumefin, "\n") == 0);
	    }

	    print_if<PROGRESS>("Loaded %d assumptions from the assume file.\n", assume_arr.size());
	    fclose(assumefin);
	    check_uniqueness();
	}
    
    victory check(const binconf &bc) const
	{
	    const auto lookup = victory_map.find(bc.hash_with_last());
	    if (lookup != victory_map.end())
	    {
		// print_if<PROGRESS>("Assumption found for binconf ");
		// print_binconf<PROGRESS>(bc);
		return (lookup->second);
	    }
	    return victory::uncertain;
	}
};

#endif
