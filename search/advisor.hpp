#ifndef _ADVISOR_HPP
#define _ADVISOR_HPP

#include "binconf.hpp"
#include "filetools.hpp"

// One specific advice point.
class advice
{
    
public:
    binconf bc;
    bin_int suggestion;

    bool match(const binconf* other) const
	{
	    return binconf_equal(&bc, other);
	}
};

class advisor
{
private:
    std::vector<advice> adv_arr;
public:
    bin_int suggest_advice(const binconf* configuration) const
	{
	    for (const advice& adv: adv_arr)
	    {
		if (adv.match(configuration))
		{
		    return adv.suggestion;
		}
	    }

	    return 0;
	}

    // Check that all hashes of the array appear once. This is a sufficient, if slightly unreliable,
    // test of uniqueness.
    
    void check_uniqueness() const
	{
	    std::vector<uint64_t> hashes;
	    for (auto& advice: adv_arr)
	    {
		hashes.push_back(advice.bc.hash_with_last());
	    }

	    std::sort(hashes.begin(), hashes.end());
	    const auto duplicate = std::adjacent_find(hashes.begin(), hashes.end());
	    
	    if (duplicate != hashes.end())
	    {
		fprintf(stderr, "Duplicate entry in advice list: ");
		for (auto& advice: adv_arr)
		{
		    if (advice.bc.hash_with_last() == *duplicate)
		    {
			print_binconf_stream(stderr, advice.bc);
			exit(-1);
		    }
		}
	    }
	}

    void load_advice_file(const char* filename)
	{
	    FILE* advicefin = fopen(filename, "r");
	    if (advicefin == NULL)
	    {
		ERROR("Unable to open file %s\n", filename);
	    }

	    while(!feof(advicefin))
	    {
		// Using filetools functions to load the bin configuration part.
		std::array<bin_int, BINS+1> loads = load_segment_with_loads(advicefin);
		std::array<bin_int, S+1> items = load_segment_with_items(advicefin);
		bin_int last_item = load_last_item_segment(advicefin);
		binconf curbc(loads, items, last_item);
		
		// Load the suggestion.
		bin_int suggestion = 0;
		if(fscanf(advicefin, " suggestion: %" SCNd16, &suggestion) != 1)
		{
		    ERROR("Suggestion %d failed to load.\n", adv_arr.size());
		}

		curbc.consistency_check();
		
		advice adv; adv.bc = curbc; adv.suggestion = suggestion;
		adv_arr.push_back(adv);
		assert(fscanf(advicefin, "\n") == 0);
	    }

	    print_if<PROGRESS>("Loaded %d suggestions from the advice file.\n", adv_arr.size());
	    fclose(advicefin);
	    check_uniqueness();
	}
};

#endif
