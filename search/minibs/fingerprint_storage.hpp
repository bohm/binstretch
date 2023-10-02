#pragma once

#include "parallel_hashmap/phmap.h"
#include "fingerprints.hpp"


class fingerprint_storage {
public:

    // flat_hash_map<size_t, int> refcounts{};
    // flat_hash_map<size_t, uint64_t> fp_hashes{};

    flat_hash_map<uint64_t, augmented_fp_set> fingerprints{};
    flat_hash_map<uint64_t, size_t> loadhash_representative_fp{};
    std::vector<uint64_t> *rns = nullptr;

    explicit fingerprint_storage(std::vector<uint64_t> *irns) {
        rns = irns;
    }

    void add_partition_to_loadhash(uint64_t loadhash, unsigned int winning_partition_index) {
        uint64_t previous_hash = 0;
        if (loadhash_representative_fp.contains(loadhash)) {
            previous_hash = fingerprints[loadhash_representative_fp[loadhash]].hash;
        }

        uint64_t new_fp_hash = fingerprint_virtual_hash(rns, previous_hash, winning_partition_index);

        if (fingerprints.contains(new_fp_hash)) {
            if (loadhash_representative_fp.contains(loadhash)) {
                fingerprints[loadhash_representative_fp[loadhash]].refcount--;
            }
            loadhash_representative_fp[loadhash] = new_fp_hash;
            fingerprints[new_fp_hash].refcount++;
        } else {
            if (loadhash_representative_fp.contains(loadhash)) {
                augmented_fp_set afs(fingerprints[loadhash_representative_fp[loadhash]].fp, new_fp_hash);
                afs.fp.insert(winning_partition_index);
                afs.refcount++;
                fingerprints[loadhash_representative_fp[loadhash]].refcount--;
                fingerprints[new_fp_hash] = afs;
                loadhash_representative_fp[loadhash] = new_fp_hash;
            } else {
                augmented_fp_set afs;
                afs.fp.insert(winning_partition_index);
                afs.hash = new_fp_hash;
                afs.refcount++;
                fingerprints[new_fp_hash] = afs;
                loadhash_representative_fp[loadhash] = new_fp_hash;
            }
        }
    }

    // Returns the fingerprint on query or nullptr.
    fingerprint_set * query_fp(uint64_t loadhash) {
         if (!loadhash_representative_fp.contains(loadhash))
         {
             return nullptr;
         } else {
             return &(fingerprints[loadhash_representative_fp[loadhash]].fp);
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

    void output(flat_hash_map<uint64_t, unsigned int>& out_fingerprint_map,
            std::vector<fingerprint_set*> &out_fp_vector) {
        out_fp_vector.clear();
        out_fingerprint_map.clear();

        flat_hash_map<uint64_t, unsigned int> position_in_out_vector;
        for (auto &[key, afp]: fingerprints) {
            auto* output_fp = new fingerprint_set(afp.fp);
            uint64_t hash = fingerprint_hash(*rns, afp.fp);
            out_fp_vector.emplace_back(output_fp);
            position_in_out_vector[hash] = out_fp_vector.size() - 1;
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
        fingerprints.clear();
    }

    // debug functions

    flat_hash_map<uint64_t, size_t> compute_frequencies() {
        flat_hash_map<uint64_t, size_t> freq;
        for (auto & [loadhash, hash] : loadhash_representative_fp) {
            freq[loadhash] = fingerprints[hash].fp.size();
        }
        return freq;
    }

    std::vector<std::pair<int, flat_hash_set<uint64_t>>> positions_to_check{};

    void add_positions_to_check(unsigned int layer_index, const flat_hash_set<uint64_t>& winning_loadhashes) {
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
};