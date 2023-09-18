#pragma once

#include "parallel_hashmap/phmap.h"
#include "fingerprints.hpp"


class fingerprint_storage {
public:

    flat_hash_map<size_t, int> refcounts{};

    flat_hash_map<size_t, fingerprint_set> fingerprints{};
    flat_hash_map<uint64_t, size_t> loadhash_representative_fp{};
    std::vector<uint64_t> *rns{};

    explicit fingerprint_storage(std::vector<uint64_t> *irns) {
        rns = irns;
    }

    // Returns the hash of the new afp.
    size_t increase(const fingerprint_set &fp) {
        size_t hash = fingerprint_hash(*rns, fp);
        if (fingerprints.contains(hash)) {
            refcounts[hash]++;
        } else {
            refcounts[hash] = 1;
            fingerprints[hash] = fp;
        }
        return hash;
    }

    void decrease(const fingerprint_set &fp) {
        size_t hash = fingerprint_hash(*rns, fp);
        refcounts[hash]--;
    }

    void change_representative(uint64_t loadhash, const fingerprint_set &newfp) {
        if (loadhash_representative_fp.contains(loadhash)) {
            size_t old_representative_hash = loadhash_representative_fp[loadhash];
            fingerprint_set& fp_old = fingerprints[old_representative_hash];
            decrease(fp_old);
        }
        size_t new_representative_hash = increase(newfp);
        loadhash_representative_fp[loadhash] = new_representative_hash;

    }

    // Returns the fingerprint on query or nullptr.
    fingerprint_set * query_fp(uint64_t loadhash) {
         if (!loadhash_representative_fp.contains(loadhash))
         {
             return nullptr;
         } else {
             return &(fingerprints[loadhash_representative_fp[loadhash]]);
         }
    }

    // Garbage collection of fingerprints with zero references.
    void collect() {
        std::vector<size_t> marked_for_deletion;
        for (auto& [hash, count] : refcounts) {
            if (count <= 0) {
                marked_for_deletion.emplace_back(hash);
            }
        }

        for (size_t hash: marked_for_deletion)
        {
            fingerprints.erase(hash);
            refcounts.erase(hash);
        }
    }

    void output(flat_hash_map<uint64_t, unsigned int>& out_fingerprint_map,
            std::vector<fingerprint_set*> &out_fp_vector) {
        out_fp_vector.clear();
        out_fingerprint_map.clear();

        flat_hash_map<size_t, unsigned int> position_in_out_vector;
        for (auto &[key, afp]: fingerprints) {
            auto* output_fp = new fingerprint_set(afp);
            size_t hash = fingerprint_hash(*rns, afp);
            out_fp_vector.emplace_back(output_fp);
            position_in_out_vector[hash] = out_fp_vector.size() - 1;
        }

        for (auto &[loadhash, hash] : loadhash_representative_fp) {
            out_fingerprint_map[loadhash] = position_in_out_vector[hash];
        }
        loadhash_representative_fp.clear();
        fingerprints.clear();
    }

    // debug functions

    flat_hash_map<uint64_t, size_t> compute_frequencies() {
        flat_hash_map<uint64_t, size_t> freq;
        for (auto & [loadhash, hash] : loadhash_representative_fp) {
            freq[loadhash] = fingerprints[hash].size();
        }
        return freq;
    }
};