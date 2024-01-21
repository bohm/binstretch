#pragma once

#include "parallel_hashmap/phmap.h"
#include "fingerprints.hpp"


class fingerprint_storage {
public:

    // flat_hash_map<size_t, int> refcounts{};
    // flat_hash_map<size_t, uint64_t> fp_hashes{};

    flat_hash_map<uint64_t, unsigned short> fp_by_itemhash{};
    flat_hash_map<unsigned short, augmented_fp_set> fingerprints{};
    unsigned short max_id = 0;

    flat_hash_map<index_t, unsigned short> loadhash_representative_fp{};
    std::vector<uint64_t> *rns = nullptr;

    explicit fingerprint_storage(std::vector<uint64_t> *irns) {
        rns = irns;
    }

    void add_partition_to_load_index(index_t load_index, unsigned int winning_partition_index) {
        uint64_t previous_fp_hash = 0;
        if (loadhash_representative_fp.contains(load_index)) {
            previous_fp_hash = fingerprints[loadhash_representative_fp[load_index]].hash;
        }

        uint64_t new_fp_hash = fingerprint_virtual_hash(rns, previous_fp_hash, winning_partition_index);

        if (fp_by_itemhash.contains(new_fp_hash)) {
            unsigned short existing_equal_fp_id = fp_by_itemhash[new_fp_hash];
            if (loadhash_representative_fp.contains(load_index)) {
                fingerprints[loadhash_representative_fp[load_index]].refcount--;
            }
            loadhash_representative_fp[load_index] = existing_equal_fp_id;
            fingerprints[existing_equal_fp_id].refcount++;
        } else {
            if (loadhash_representative_fp.contains(load_index)) {
                augmented_fp_set afs(fingerprints[loadhash_representative_fp[load_index]].fp, new_fp_hash);
                afs.fp.insert(winning_partition_index);
                afs.refcount = 1;
                fingerprints[loadhash_representative_fp[load_index]].refcount--;
                fingerprints[max_id] = afs;
                fp_by_itemhash[new_fp_hash] = max_id;
                loadhash_representative_fp[load_index] = max_id;
                max_id++;
            } else {
                augmented_fp_set afs;
                afs.fp.insert(winning_partition_index);
                afs.hash = new_fp_hash;
                afs.refcount = 1;
                fingerprints[max_id] = afs;
                fp_by_itemhash[new_fp_hash] = max_id;
                loadhash_representative_fp[load_index] = max_id;
                max_id++;
            }

            if (max_id == std::numeric_limits<unsigned short>::max()) {
                fprintf(stderr, "The number of unique trees hit 65535, we have to terminate.\n");
                exit(-1);
            }
        }
    }

    // Returns the fingerprint on query or nullptr.
    fingerprint_set * query_fp(index_t load_index) {
         if (!loadhash_representative_fp.contains(load_index))
         {
             return nullptr;
         } else {
             return &(fingerprints[loadhash_representative_fp[load_index]].fp);
         }
    }

    // A full query on a pair load/layer.
    bool query(index_t load_index, unsigned int layer_index) {
        if (!loadhash_representative_fp.contains(load_index))
        {
            return false;
        } else {
            return fingerprints[loadhash_representative_fp[load_index]].fp.contains(layer_index);
        }
    }

    // Garbage collection of fingerprints with zero references.
    void collect() {
        std::vector<size_t> marked_for_deletion;
        for (auto& [afp_hash, afp]: fingerprints) {
            if (afp.refcount <= 0) {
                marked_for_deletion.emplace_back(afp_hash);
            }
        }

        for (size_t hash: marked_for_deletion)
        {
            fingerprints.erase(hash);
        }
    }

    // Another form of garbage collection, where we recompute the max_ids to be continuous in [0, fingerprint.size() ).
    void reindex_fingerprints() {
        if (fingerprints.empty()) {
            return;
        }

        unsigned short id_counter = 0;
        flat_hash_map<unsigned short, unsigned short> reindexing;
        for (auto &[key, _]: fingerprints) {
            reindexing[key] = id_counter;
            id_counter++;
        }

        // Clone fingerprints and re-insert with new keys.
        flat_hash_map<unsigned short, augmented_fp_set> fp_clone(fingerprints);
        fingerprints.clear();
        for (auto &[key, value]: fp_clone) {
            fingerprints[reindexing[key]] = value;
        }

        for (auto &[key, value]: fp_by_itemhash) {
            fp_by_itemhash[key] = reindexing[value];
        }

        for (auto &[key, value]: loadhash_representative_fp) {
            loadhash_representative_fp[key] = reindexing[value];
        }

        max_id = id_counter;
    }

    void output(flat_hash_map<uint32_t, unsigned short>& out_fingerprint_map,
            std::vector<fingerprint_set*> &out_fp_vector) {
        out_fp_vector.clear();
        out_fingerprint_map.clear();


        flat_hash_map<unsigned short, unsigned short> position_in_out_vector;
        for (auto &[key, afp]: fingerprints) {
            unsigned short old_fingerprint_index = key;
            auto* output_fp = new fingerprint_set(afp.fp);
            out_fp_vector.emplace_back(output_fp);
            position_in_out_vector[old_fingerprint_index] = (unsigned short) (out_fp_vector.size() - 1);
        }

        for (auto &[loadhash, hash] : loadhash_representative_fp) {
            out_fingerprint_map[loadhash] = position_in_out_vector[hash];
        }

        // Strict consistency check.

        /*
        for (const auto &[layer_index, winning_loadhashes] : positions_to_check)
        {
            for (uint64_t win_loadhash: winning_loadhashes) {
                assert(out_fingerprint_map.contains(win_loadhash));
                assert(out_fp_vector[out_fingerprint_map[win_loadhash]]->contains(layer_index));
            }
        }
         */

        loadhash_representative_fp.clear();
        fp_by_itemhash.clear();
        fingerprints.clear();
        max_id = 0;
    }

    // debug functions

    flat_hash_map<uint32_t, size_t> compute_frequencies() {
        flat_hash_map<uint32_t, size_t> freq;
        for (auto & [loadhash, hash] : loadhash_representative_fp) {
            freq[loadhash] = fingerprints[hash].fp.size();
        }
        return freq;
    }

    std::vector<std::pair<int, flat_hash_set<uint32_t>>> positions_to_check{};

    void add_positions_to_check(unsigned int layer_index, const flat_hash_set<uint32_t>& winning_loadhashes) {
        positions_to_check.emplace_back(layer_index, winning_loadhashes);
    }

    void check_wins() {
        for (const auto &[layer_index, winning_loadhashes] : positions_to_check)
        {
            for (uint64_t win_loadhash: winning_loadhashes) {
                assert(loadhash_representative_fp.contains(win_loadhash));
                assert(query_fp(win_loadhash)->contains(layer_index));
            }
        }
    }

    void stats(int pass = -1) const {
        if (pass >= 0) {
            fprintf(stderr, "Fingerprint stats after layer %d:\n", pass);
        }

        fprintf(stderr, "Number of unique fingerprint sets: %zu.\n", fingerprints.size());
        fprintf(stderr, "Number of loadhashes associated with >= 1 winning itemhash: %zu.\n",
                loadhash_representative_fp.size());
        uint64_t itemhashes_total = 0;
        for (const auto& [key, value] : fingerprints) {
            itemhashes_total += value.fp.size();
        }
        fprintf(stderr, "Total itemhashes in all fingerprints: %" PRIu64 ", average %Lf.\n",
                itemhashes_total, itemhashes_total / (long double) fingerprints.size());
    }
};