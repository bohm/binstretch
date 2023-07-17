#pragma once

class fingerprint_manager
{
public:
    // Fingerprints on their own is just a transposition of the original minibs
    // approach. Instead of storing loadhashes for each item layer, we store
    // list of item layer ids for every loadhash.

    // The memory saving advantage comes from sparsifiying this structure.
    // This sparsification is not easily done on the fly, so we do it during
    // postprocessing.


    // Fingerprint map stores for every loadhash the possibly non-unique
    // set of itemconfs (itemhashes) for which this loadhash (load configuration)
    // is winning.
    // It should always point to a tree. This tree may be shared.
    
    flat_hash_map<uint64_t, unsigned int> fingerprint_map;
    // We store pointers to the winning fingerprints,
    // to enable smooth sparsification.
    std::vector<shared_ptr<fp_set>> fingerprints;

    // Vector of unique fingerprints, created during sparsification.
    // Useful for deleting them e.g. during termination.
    std::vector<shared_ptr<fp_set>> unique_fps;


    uint64_t fingerprint_hash(flat_hash_set<unsigned int> * fp)
	{
	    uint64_t ret = 0;
	    for (unsigned int u: *fp)
	    {
		ret ^= (*rns_for_fingerprints)[u];
	    }
	    return ret;
	}
    

    // Creates a clone for the tree at this position.
    // This will take more memory and necessitates rerunning the sparsification.
    void prepare_insertion(uint64_t loadhash)
	{
	    unsigned int fingerprint_pos = fingerprint_map[loadhash];
	    shared_ptr<fp_set> fp_old = fingerprints[fingerprint_pos];
	    shared_ptr<fp_set> fp_new = new fp_set(*fp_old);
	    fingerprints[fingerprint_pos] = fp_new;
	    // fingerprint_map remains unchanged 
	}

    void create_empty_set(const uint64_t loadhash)
	{
	    // assert(!fingerprint_map.contains(loadhash));
	    unsigned int new_pos = fingerprints.size();
	    shared_ptr<fp_set> fp_new = new fp_set();
	    fingerprints.push_back(fp_new);
	    fingerprint_map[loadhash] = new_pos;
	}


    void encache_alg_win(const uint64_t loadhash, const unsigned int item_layer_id)
	{
	    if (fingerprint_map.contains(loadhash))
	    {
		prepare_insertion(loadhash);
	    } else
	    {
	        create_empty_set(loadhash);
	    }

	    fingerprints[fingerprint_map[loadhash]]->insert(item_layer_id);
	}

    inline bool query_fingerprint_winning(const uint64_t loadhash, const unsigned int item_layer_id)
	{
	    return fingerprints[fingerprint_map[loadhash]]->contains(item_layer_id);
	}


    void sparsify(flat_hash_map<uint64_t, shared_ptr<fp_set>>* unique_fingerprint_map = nullptr)
	{
	    bool custom_allocation = false;
	    if (unique_fingerprint_map == nullptr)
	    {
		custom_allocation = true;
		unique_fingerprint_map = new flat_hash_map<uint64_t, shared_ptr<fp_set>>();
	    }
	    else
	    {
		unique_fingerprint_map->clear();
	    }
	    
	    for (unsigned int i = 0; i < fingerprints.size(); ++i)
	    {
		uint64_t hash = fingerprint_hash(fingerprints[i]);
		if (unique_fingerprint_map->contains(hash))
		{
			fingerprints[i] = (*unique_fingerprint_map)[hash];
		} else
		{
		    (*unique_fingerprint_map)[hash] = fingerprints[i];
		}
	    }


	    if (custom_allocation)
	    {
		unique_fingerprint_map->clear();
		delete unique_fingerprint_map;
	    }
	}

    void build_unique_vector()
	{
	    flat_hash_map<uint64_t, shared_ptr<fp_set>> * unique_fingerprint_map =
		new flat_hash_map<uint64_t, shared_ptr<fp_set>>();
	    sparsify(unique_fingerprint_map);

	    unique_fps.clear();
	    // Convert the flat_hash_map into a vector, keeping track of unique elements
	    // and forgetting the hash function.
	    for (const auto& [key, el]: *unique_fingerprint_map)
	    {
		unique_fps.push_back(el);
	    }

	    unique_fingerprint_map->clear();
	    delete unique_fingerprint_map;
	}


    
 
};
