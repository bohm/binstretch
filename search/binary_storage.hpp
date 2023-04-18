#pragma once

// Allows for recovery of some precomputed cache tables, primarily minibinstretching.

#include "common.hpp"
#include <filesystem>

template <int DENOMINATOR> class binary_storage
{
public:

    char storage_file_path[256];
    FILE *storage_file = nullptr;
    binary_storage()
	{
	    sprintf(storage_file_path, "./cache/minibs-%d-%d-%d-scale-%d.bin", BINS, R, S, DENOMINATOR);
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
	    succ_read_bins = fread(&read_bins, sizeof(int), 1, storage_file);
	    succ_read_r = fread(&read_r, sizeof(int), 1, storage_file);
	    succ_read_s = fread(&read_s, sizeof(int), 1, storage_file);
	    succ_read_scale = fread(&read_scale, sizeof(int), 1, storage_file);

	    if (succ_read_bins != 1 || succ_read_r != 1 || succ_read_s != 1 || succ_read_scale != 1)
	    {
		ERRORPRINT("Binary_storage read %d %d %d %d: Signature reading failed.\n", BINS, R, S, DENOMINATOR);
	    }
	    
	    if (read_bins != BINS || read_r != R || read_s != S || read_scale != DENOMINATOR)
	    {
		fprintf(stderr, "Reading the storage of results: signature verification failed.\n");
		fprintf(stderr, "Signature read: %d %d %d %d", read_bins, read_r, read_s, read_scale);
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

   
    void write_one_set(std::unordered_set<uint64_t> &s)
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

    void read_one_set(std::unordered_set<uint64_t>& out_set)
	{
	    out_set.clear();
	    unsigned int set_size = read_set_size();
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

    void write_set_system(std::vector<std::unordered_set<uint64_t>>& system)
	{
	    write_number_of_sets(system.size());
	    for (std::unordered_set<uint64_t> &set : system)
	    {
		write_one_set(set);
		write_delimeter();
	    }
	}
    
    void read_set_system(std::vector<std::unordered_set<uint64_t>>& out_system)
	{
	    unsigned int nos = read_number_of_sets();
	    out_system.clear();
	    for (unsigned int i = 0; i < nos; i++)
	    {
		out_system.push_back(std::unordered_set<uint64_t>());
		read_one_set(out_system[i]);
		read_delimeter();
	    }
	}
    
    void restore(std::vector<std::unordered_set<uint64_t>>& out_system)
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

	    read_set_system(out_system);
	    close();
	}

    void backup(std::vector<std::unordered_set<uint64_t>>& system)
	{
	    open_for_writing();
	    write_signature();
	    write_zobrist_table();
	    write_set_system(system);
	    close();
	}
};
