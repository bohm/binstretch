#pragma once

// Allows for recovery of some precomputed cache tables, primarily minibinstretching.
#include <filesystem>
#include <parallel_hashmap/phmap.h>
// Notice: https://github.com/greg7mdp/parallel-hashmap is now required for the program to build.
// This is a header-only hashmap/set that seems quicker and lower-memory than the unordered_set.

#include "../common.hpp"
#include "../functions.hpp"
#include "itemconfig.hpp"

using phmap::flat_hash_set;
using phmap::flat_hash_map;

template <int DENOMINATOR> class binary_storage
{
public:

    const int VERSION = 4;
    char storage_file_path[256];
    FILE *storage_file = nullptr;
    binary_storage()
	{
	    sprintf(storage_file_path, "./cache/minibs-%d-%d-%d-scale-%d-v%d.bin", BINS, R, S, DENOMINATOR, VERSION);
	}

    bool storage_exists()
	{
	    return std::filesystem::exists(storage_file_path);
	}
    void open_for_writing()
	{
	    storage_file = fopen(storage_file_path, "wb");
	    assert(storage_file != nullptr);
	}

    void open_for_reading()
	{
	    storage_file = fopen(storage_file_path, "rb");
	    assert(storage_file != nullptr);
	}
    void close()
	{
	    fclose(storage_file);
	    storage_file = nullptr;
	}

    bool check_signature()
	{
	    int read_bins = 0, read_r = 0, read_s = 0, read_scale = 0;
	    int succ_read_bins = 0, succ_read_r = 0, succ_read_s = 0, succ_read_scale = 0;
	    int read_version = 0, succ_read_version = 0;
	    succ_read_bins = fread(&read_bins, sizeof(int), 1, storage_file);
	    succ_read_r = fread(&read_r, sizeof(int), 1, storage_file);
	    succ_read_s = fread(&read_s, sizeof(int), 1, storage_file);
	    succ_read_scale = fread(&read_scale, sizeof(int), 1, storage_file);
	    succ_read_version = fread(&read_version, sizeof(int), 1, storage_file);

	    if (succ_read_bins != 1 || succ_read_r != 1 || succ_read_s != 1 ||
		succ_read_scale != 1 || succ_read_version != 1)
	    {
		ERRORPRINT("Binary_storage read %d %d %d %d v%d: Signature reading failed.\n", BINS, R, S, DENOMINATOR, VERSION);
	    }
	    
	    if (read_bins != BINS || read_r != R || read_s != S || read_scale != DENOMINATOR || read_version != VERSION)
	    {
		fprintf(stderr, "Reading the storage of results: signature verification failed.\n");
		fprintf(stderr, "Signature read: %d %d %d %d v%d", read_bins, read_r, read_s, read_scale, read_version);
		// assert(read_bins == BINS && read_r == R && read_s == S);
		return false;
	    }

	    return true;
	}

    void write_signature()
	{
	    constexpr int D = DENOMINATOR;
	    fwrite(&BINS, sizeof(int), 1, storage_file);
	    fwrite(&R, sizeof(int), 1, storage_file);
	    fwrite(&S, sizeof(int), 1, storage_file);
	    fwrite(&D, sizeof(int), 1, storage_file);
	    fwrite(&VERSION, sizeof(int), 1, storage_file);
	}

    void write_zobrist_table()
	{
	    fwrite(Zi, sizeof(uint64_t), ZI_SIZE, storage_file);
	    fwrite(Zl, sizeof(uint64_t), ZL_SIZE, storage_file);
	}

    bool check_zobrist_table()
	{
	    bool ret = true;
	    uint64_t* read_zi = new uint64_t[ZI_SIZE];
    	    uint64_t* read_zl = new uint64_t[ZL_SIZE];
	    int succ_read_zi = 0, succ_read_zl = 0;
	    succ_read_zi = fread(read_zi, sizeof(uint64_t), ZI_SIZE, storage_file);
	    succ_read_zl = fread(read_zl, sizeof(uint64_t), ZL_SIZE, storage_file);

	    if (succ_read_zi != ZI_SIZE || succ_read_zl != ZL_SIZE)
	    {
		ERRORPRINT("Binary_storage read %d %d %d %d: Zobrist table read failed.\n", BINS, R, S, DENOMINATOR);
	    }
	    for (int i = 0; i < ZI_SIZE; i++)
	    {
		if (Zi[i] != read_zi[i])
		{
		    fprintf(stderr, "Zobrist table Zi mismatch at %d", i);
		    ret = false;
		    break;
		}
	    }

	    for (int j = 0; j < ZL_SIZE; j++)
	    {
		if (Zl[j] != read_zl[j])
		{
		    fprintf(stderr, "Zobrist table Zl mismatch at %d", j);
		    ret = false;
		    break;
		}
	    }

	    delete[] read_zi;
	    delete[] read_zl;
	    return ret;
	}

    void write_delimeter()
	{
	    int del = -1;
	    fwrite(&del, sizeof(int), 1, storage_file);
	}

    void read_delimeter()
	{
	    int del = 0;
	    int del_read = 0;
	    del_read = fread(&del, sizeof(int), 1, storage_file);
	    if (del != -1 || del_read != 1)
	    {
		ERRORPRINT("Binary storage error: missing delimeter.\n");
	    }
	}
    
    void write_number_of_sets(unsigned int nos)
	{
	    fwrite(&nos, sizeof(unsigned int), 1, storage_file);
	}

    unsigned int read_number_of_sets()
	{
	    unsigned int nos = 0;
	    int nos_read = 0;
	    nos_read = fread(&nos, sizeof(unsigned int), 1, storage_file);
	    if (nos_read != 1)
	    {
		ERRORPRINT("Binary storage error: failed to read the number of sets.\n");
	    }
	    return nos;
	}
 
    void write_set_size(unsigned int set_size)
	{
	    fwrite(&set_size, sizeof(unsigned int), 1, storage_file);
	}

    unsigned int read_set_size()
	{
	    int set_size = 0, set_size_read = 0;
	    set_size_read = fread(&set_size, sizeof(unsigned int), 1, storage_file);

	    if (set_size_read != 1)
	    {
		ERRORPRINT("Binary storage error: failed to read the set size.\n");
	    }

	    return set_size;
	}

   
    void write_one_set(flat_hash_set<uint64_t> &s)
	{
	    write_set_size(s.size());
	    uint64_t* set_as_array = new uint64_t[s.size()];
	    uint64_t c = 0;
	    for (const uint64_t& el: s)
	    {
		set_as_array[c++] = el;
	    }

	    fwrite(set_as_array, sizeof(uint64_t), c, storage_file);
	    delete[] set_as_array;
	}

       
    void write_one_set(flat_hash_set<unsigned int> *s)
	{
	    write_set_size(s->size());
	    unsigned int* set_as_array = new unsigned int[s->size()];
	    uint64_t c = 0;
	    for (const unsigned int & el: *s)
	    {
		set_as_array[c++] = el;
	    }

	    fwrite(set_as_array, sizeof(unsigned int), c, storage_file);
	    delete[] set_as_array;
	}


    void read_one_set(flat_hash_set<uint64_t>& out_set)
	{
	    out_set.clear();
	    unsigned int set_size = read_set_size();
	    out_set.reserve(set_size);
	    size_t set_read = 0;
	    uint64_t* set_as_array = new uint64_t[set_size];
 	    set_read = fread(set_as_array, sizeof(uint64_t), set_size, storage_file);

	    if (set_read != set_size)
	    {
		ERRORPRINT("Binary storage error: failed to read one of the sets.\n");
	    }
	    
	    for (unsigned int i = 0; i < set_size; i++)
	    {
		out_set.insert(set_as_array[i]);
	    }

	    delete[] set_as_array;
	}

     void read_one_set(flat_hash_set<unsigned int>* out_set)
	{
	    out_set->clear();
	    unsigned int set_size = read_set_size();
	    out_set->reserve(set_size);
	    size_t set_read = 0;
	    unsigned int* set_as_array = new unsigned int[set_size];
 	    set_read = fread(set_as_array, sizeof(unsigned int), set_size, storage_file);

	    if (set_read != set_size)
	    {
		ERRORPRINT("Binary storage error: failed to read one of the sets.\n");
	    }
	    
	    for (unsigned int i = 0; i < set_size; i++)
	    {
		out_set->insert(set_as_array[i]);
	    }

	    delete[] set_as_array;
	}

   

    void write_set_system(std::vector<flat_hash_set<uint64_t>>& system)
	{
	    write_number_of_sets(system.size());
	    for (flat_hash_set<uint64_t> &set : system)
	    {
		write_one_set(set);
		write_delimeter();
	    }
	}
    
    void read_set_system(std::vector<flat_hash_set<uint64_t>>& out_system)
	{
	    unsigned int nos = read_number_of_sets();
	    out_system.clear();
	    for (unsigned int i = 0; i < nos; i++)
	    {
		out_system.push_back(flat_hash_set<uint64_t>());
		read_one_set(out_system[i]);
		read_delimeter();
	    }
	}


    void write_unique_fingerprints(std::vector<flat_hash_set<unsigned int>* >& unique_fps,
				   flat_hash_map<unsigned int, flat_hash_set<unsigned int> *>& p_seq)
	{
	    write_set_size(unique_fps.size());

	    for (unsigned int i = 0; i < p_seq.size(); ++i)
	    {
		write_one_set(p_seq[i]);
		write_delimeter();
	    }
	}


    // Reads the fingerprints in order, creates the mapping as the fingerprints get allocated.
    void read_unique_fingerprints(
	std::vector<flat_hash_set<unsigned int>* >& out_unique_fps,
	flat_hash_map<unsigned int, flat_hash_set<unsigned int> *>& out_p_seq)
	{
	    unsigned int unique_trees = read_set_size();
	    for (unsigned int i = 0; i < unique_trees; ++i)
	    {
		flat_hash_set<unsigned int>* fp = new flat_hash_set<unsigned int>();
		read_one_set(fp);
		out_p_seq[i] = fp;
		out_unique_fps.push_back(fp);
		read_delimeter();
	    }
	    
	}

    unsigned int * undo_fp_pointers(std::vector< flat_hash_set<unsigned int>* >& fingerprints,
				    flat_hash_map<flat_hash_set<unsigned int> *, unsigned int>& reverse_pointers)
	{
	    unsigned int *ret = new unsigned int[fingerprints.size()];
	    for (unsigned int i = 0; i < fingerprints.size(); ++i)
	    {
		ret[i] = reverse_pointers[fingerprints[i]];
	    }

	    return ret;
	    
	}

    std::pair<unsigned int, unsigned int*> read_pointerless_fingerprints()
	{
	    unsigned int fingerprints_size = read_set_size();
	    unsigned int* pointerless = new unsigned int[fingerprints_size];
	    
    	    int read_prints = fread(pointerless, sizeof(unsigned int),
		   fingerprints_size, storage_file);

	    if (read_prints != (int) fingerprints_size)
	    {
		ERRORPRINT("Binary storage error: failed to read pointerless fingerprints.\n");
	    }
	    read_delimeter();

	    return std::pair(fingerprints_size, pointerless);
    
	}
    
    void convert_pointerless_to_pointers(unsigned int fingerprint_count,
					 unsigned int* pointerless_fingerprints,
					 flat_hash_map<unsigned int, flat_hash_set<unsigned int> *>& p_seq,
					 std::vector< flat_hash_set<unsigned int> *>& out_fingerprints)
	{
	    for (unsigned int i = 0; i < fingerprint_count; ++i)
	    {
		out_fingerprints.push_back(p_seq[pointerless_fingerprints[i]]);
	    }
	}
	  
    void read_fingerprint_map(flat_hash_map<uint64_t, unsigned int>& out_fingerprint_map)
	{
	    unsigned int map_size = read_set_size();
	    uint64_t *keys = new uint64_t[map_size];
	    unsigned int *values = new unsigned int[map_size];

 	    int succ_read_keys = fread(keys, sizeof(uint64_t), map_size, storage_file);

 	    if (succ_read_keys != (int) map_size)
	    {
		ERRORPRINT("Binary storage error: failed to read the keys of fingerprint_map.\n");
	    }

	    read_delimeter();

	    int succ_read_values =  fread(values, sizeof(unsigned int), map_size, storage_file);
	    if (succ_read_values != (int) map_size)
	    {
		ERRORPRINT("Binary storage error: failed to read the values of fingerprint_map.\n");
	    }

	    read_delimeter();

	    for (unsigned int i = 0; i < map_size; ++i)
	    {
		out_fingerprint_map[keys[i]] = values[i];
	    }
	}
   

    void write_fingerprint_map(flat_hash_map<uint64_t, unsigned int>& fingerprint_map)
	{
	    // Store the map as two arrays.

	    uint64_t *keys = new uint64_t[fingerprint_map.size()];
	    unsigned int *values = new unsigned int[fingerprint_map.size()];

	    int pos = 0;
	    for( const auto& [k, v]: fingerprint_map)
	    {
		keys[pos] = k;
		values[pos] = v;
		pos++;
	    }

	    // Store the two arrays.
	    write_set_size(fingerprint_map.size());
	    
    	    fwrite(keys, sizeof(uint64_t), fingerprint_map.size(), storage_file);
	    write_delimeter();
    	    fwrite(values, sizeof(unsigned int), fingerprint_map.size(), storage_file);
	    write_delimeter();
	}
    

     void read_fingerprint_system(flat_hash_map<uint64_t, unsigned int>& out_fingerprint_map,
				  std::vector< flat_hash_set<unsigned int>* >& out_fingerprints,
				  std::vector<flat_hash_set<unsigned int>* >& out_unique_fps)
	{
	    flat_hash_map<unsigned int, flat_hash_set<unsigned int>* > pointer_sequence;
	    read_unique_fingerprints(out_unique_fps, pointer_sequence);

	    // Read pointerless fingerprints
	    auto [fingerprints_count, pointerless] = read_pointerless_fingerprints();
	    convert_pointerless_to_pointers(fingerprints_count, pointerless, pointer_sequence,
					    out_fingerprints);

	    delete[] pointerless;
	    read_fingerprint_map(out_fingerprint_map);
    
	}
	   
    void write_fingerprint_system(flat_hash_map<uint64_t, unsigned int>& fingerprint_map,
				  std::vector< flat_hash_set<unsigned int>* >& fingerprints,
				  std::vector<flat_hash_set<unsigned int>* >& unique_fps)
	{
	    flat_hash_map<unsigned int, flat_hash_set<unsigned int>* > pointer_sequence;
	    flat_hash_map<flat_hash_set<unsigned int> *, unsigned int> reverse_pointers;
	    unsigned int pointer_counter = 0;
	    for (const auto& el: unique_fps)
	    {
		pointer_sequence[pointer_counter] = el;
		reverse_pointers[el] = pointer_counter;
		pointer_counter++;
	    }

	    write_unique_fingerprints(unique_fps, pointer_sequence);
	    
	    unsigned int* pointerless_fingerprints = undo_fp_pointers(fingerprints,
								    reverse_pointers);


	    // write pointerless fingerprints;
	    write_set_size(fingerprints.size());
    	    fwrite(pointerless_fingerprints, sizeof(unsigned int),
		   fingerprints.size(), storage_file);
	    write_delimeter();

	    delete pointerless_fingerprints;
	    
	    write_fingerprint_map(fingerprint_map);
	    
	}
    
    void read_knownsum_set(flat_hash_set<uint64_t>& out_knownsum_set)
	{
	    read_one_set(out_knownsum_set);
	    read_delimeter();
	}

    void write_knownsum_set(flat_hash_set<uint64_t> &knownsum_set)
	{
	    write_one_set(knownsum_set);
	    write_delimeter();
	}

    void write_number_of_feasible_itemconfs(unsigned int nofc)
	{
	    fwrite(&nofc, sizeof(unsigned int), 1, storage_file);
	}

    unsigned int read_number_of_feasible_itemconfs()
	{
	    unsigned int nofc = 0;
	    int nofc_read = 0;
	    nofc_read = fread(&nofc, sizeof(unsigned int), 1, storage_file);
	    if (nofc_read != 1)
	    {
		ERRORPRINT("Binary storage error: failed to read the number of feasible minibs configurations.\n");
	    }
	    return nofc;
	}

    void write_itemconf(itemconfig<DENOMINATOR>& ic)
	{
	    fwrite(ic.items.data(), sizeof(int), DENOMINATOR, storage_file);
	}


    void read_itemconf(itemconfig<DENOMINATOR>& out_ic)
	{
	    int ic_read = 0;
	    ic_read = fread(out_ic.items.data(), sizeof(int), DENOMINATOR, storage_file);

	    if (ic_read != DENOMINATOR)
	    {
		ERRORPRINT("Binary storage error: failed to read one itemconfig.\n");
	    }
	}


    void write_feasible_itemconfs(std::vector<itemconfig<DENOMINATOR> > &feasible_ics)
	{
	    write_number_of_feasible_itemconfs(feasible_ics.size());
	    write_delimeter();
	    for (unsigned int i = 0; i < feasible_ics.size(); ++i)
	    {
		write_itemconf(feasible_ics[i]);
	    }
	    write_delimeter();
	}

    void read_feasible_itemconfs(std::vector<itemconfig<DENOMINATOR> > &feasible_ics)
	{
	    unsigned int nofc = read_number_of_feasible_itemconfs();
	    read_delimeter();
	    for (unsigned int i = 0; i < nofc; ++i)
	    {
		itemconfig<DENOMINATOR> new_ic;
		read_itemconf(new_ic);
		new_ic.hashinit();
		feasible_ics.push_back(new_ic);
	    }
	    read_delimeter();
    
	}
	    
    void restore(flat_hash_map<uint64_t, unsigned int>& out_fingerprint_map,
		 std::vector< flat_hash_set<unsigned int>* >& out_fingerprints,
		 std::vector<flat_hash_set<unsigned int>* >& out_unique_fps,
		 flat_hash_set<uint64_t> &out_knownsum_set,
		 std::vector< itemconfig<DENOMINATOR> >& out_feasible_ics)
	{
	    open_for_reading();
	    bool check = check_signature();
	    if (!check)
	    {
		ERRORPRINT("Error: Signature check failed!\n");
	    }

	    check = check_zobrist_table();
	    if (!check)
	    {
		ERRORPRINT("Error: The Zobrist table do not match!\n");
	    }

	    read_feasible_itemconfs(out_feasible_ics);
	    read_knownsum_set(out_knownsum_set);
	    // read_set_system(out_system);
	    read_fingerprint_system(out_fingerprint_map, out_fingerprints, out_unique_fps);
	    close();
	}

    
    void backup(
		flat_hash_map<uint64_t, unsigned int>& fingerprint_map,
		std::vector< flat_hash_set<unsigned int>* >& fingerprints,
		std::vector<flat_hash_set<unsigned int>* >& unique_fps,
		flat_hash_set<uint64_t> &knownsum_set,
		std::vector< itemconfig<DENOMINATOR> >& feasible_ics)
	{
	    open_for_writing();
	    write_signature();
	    write_zobrist_table();
	    write_feasible_itemconfs(feasible_ics);
	    write_knownsum_set(knownsum_set);
	    write_fingerprint_system(fingerprint_map, fingerprints, unique_fps);
	    close();
	}
};