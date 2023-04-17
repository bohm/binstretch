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
	    fread(&read_bins, sizeof(int), 1, storage_file);
	    fread(&read_r, sizeof(int), 1, storage_file);
	    fread(&read_s, sizeof(int), 1, storage_file);
	    fread(&read_scale, sizeof(int), 1, storage_file);
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
	    fread(read_zi, sizeof(uint64_t), ZI_SIZE, storage_file);
	    fread(read_zl, sizeof(uint64_t), ZL_SIZE, storage_file);
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
	    fread(&del, sizeof(int), 1, storage_file);
	    assert(del == -1);
	}
    
    void write_number_of_sets(unsigned int nos)
	{
	    fwrite(&nos, sizeof(unsigned int), 1, storage_file);
	}

    unsigned int read_number_of_sets()
	{
	    unsigned int nos = 0;
	    fread(&nos, sizeof(unsigned int), 1, storage_file);
	    return nos;
	}
 
    void write_set_size(unsigned int set_size)
	{
	    fwrite(&set_size, sizeof(unsigned int), 1, storage_file);
	}

    unsigned int read_set_size()
	{
	    int set_size = 0;
	    fread(&set_size, sizeof(unsigned int), 1, storage_file);
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
	    uint64_t* set_as_array = new uint64_t[set_size];
 	    fread(set_as_array, sizeof(uint64_t), set_size, storage_file);
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
	    if (check)
	    {
		fprintf(stderr, "Signature check passed.\n");
	    }
	    else
	    {
		fprintf(stderr, "Error: Signature check failed!\n");
	    }

	    check = check_zobrist_table();
	    if (check)
	    {
		fprintf(stderr, "Zobrist table matches.\n");
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
