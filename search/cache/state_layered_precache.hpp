#pragma once
#pragma once

#include <bitset>
#include "../common.hpp"
#include "state.hpp"

class state_layered_precache {
public:


    // A heuristic for when caches should be small. Targeted for 19/14. Refine later.
    static constexpr bool SMALL_CACHE(int i) {
        // For small inputs, always return false and set all caches big.
        if (S*BINS < 8 || 4*BINS - 8 < 0) {
             return false;
        }
        return i <= 8 || i >= 4*BINS - 8;
    }

    // hi -- hashtable by itemdepth.
    std::array<std::atomic<uint64_t> *, BINS*S+1> hi;

    // cache-line-friendly pre-cache. Stores 1 in a bit if there is any chance of a hash being in there.
    static constexpr uint64_t PRECACHE_LOGSIZE = 22;
    static constexpr uint64_t PRECACHE_SIZE = (1LLU << PRECACHE_LOGSIZE);

    std::array<std::bitset<PRECACHE_SIZE>, BINS*S+1> precache{};

    // std::atomic <conf_el> *ht;
    uint64_t htsize_big;
    uint64_t htsize_small = two_to(18);
    uint64_t logsize_big;
    uint64_t logsize_small = quicklog(htsize_small);
    cache_measurements meas;

    void atomic_init_point(int depth, uint64_t point) {
        std::atomic_init(&(hi[depth][point]), 0);
    }

    void parallel_init_segment(int depth, uint64_t start, uint64_t end, uint64_t size) {
        for (uint64_t i = start; i < std::min(end, size); i++) {
            atomic_init_point(depth, i);
        }
    }

    state_layered_precache(uint64_t logbytes, int threads, std::string descriptor = "") {
        assert(logbytes <= 64);
        // A hack to make the allocation comparable.
        logbytes -= 1;
        uint64_t bytes = two_to(logbytes);
        htsize_big = power_of_two_below(bytes / sizeof(conf_el));
        logsize_big = quicklog(htsize_big);
        if (S*BINS >= 8 && 4*BINS - 8 >= 0) {
            print_if<PROGRESS>(
                    "Creating %d large caches of capacity %" PRIu64 ".\n", S*BINS-16, htsize_big);
        }


        // We allocate not 2^{k} sized cache (which makes sense because of the indexing) but we add LINPROBE_LIMIT
        // extra elements for linear probing to work for the last LINPROBE_LIMIT elements of the cache itself.

        for (int idepth = 0; idepth <= S*BINS; idepth++) {
            uint64_t htsize = 0;
            if (SMALL_CACHE(idepth)) {
                htsize = htsize_small;
            } else {
                htsize = htsize_big;
            }

            hi[idepth] = new std::atomic<uint64_t>[htsize + LINPROBE_LIMIT];
            assert(hi[idepth]!= nullptr);

            uint64_t segment = htsize / threads;
            uint64_t start = 0;
            uint64_t end = std::min(htsize, segment);

            std::vector<std::thread> th;
            for (int w = 0; w < threads; w++) {
                th.emplace_back(&state_layered_precache ::parallel_init_segment, this, idepth, start, end, htsize);
                start += segment;
                end += segment;
                start = std::min(start, htsize);
                end = std::min(end, htsize);
            }

            for (int w = 0; w < threads; w++) {
                th[w].join();
            }

            // Deal with the tail.
            for (uint64_t tail = htsize; tail < htsize + LINPROBE_LIMIT; tail++) {
                atomic_init_point(idepth, tail);
            }
        }
    }

    ~state_layered_precache() {
        for (int i = 0; i <= S*BINS; i++) {
            delete[] hi[i];
        }
    }

    conf_el access(int depth, uint64_t pos) {
        return conf_el(hi[depth][pos].load(std::memory_order_relaxed));
    }

    void store(int depth, uint64_t pos, const conf_el &e) {
        hi[depth][pos].store(e._data, std::memory_order_relaxed);
    }

    uint64_t trim_large(uint64_t ha) {
        return logpart(ha, logsize_big);
    }

    uint64_t trim_small(uint64_t ha) {
        return logpart(ha, logsize_small);
    }

    size_t trim_precache(uint64_t ha) {
        return logpart(ha, PRECACHE_LOGSIZE);
    }

    // Intentionally does nothing. Only relevant for state_analysis_cache.
    void report() {

    }

    // victory lookup(uint64_t h);
    victory lookup_virtual(binconf *bc, int item, unsigned char bin);

    // void insert(conf_el e, uint64_t h);
    void insert_binconf(binconf *bc, victory algorithm_victory);

    // Functions for clearing part of entirety of the cache.

};

victory state_layered_precache::lookup_virtual(binconf *bc, int item, unsigned char bin) {
    uint64_t h = bc->virtual_hash_with_low(item, bin);
    uint64_t pos;
    int depth = bc->itemcount() + 1;

;

    if (SMALL_CACHE(depth)) {
        pos = trim_small(h);
    } else {
        pos = trim_large(h);
    }

    // Use linear probing to check for the hashed value.
    for (uint64_t i = 0; i < LINPROBE_LIMIT; i++) {
        // assert(pos + i < size());
        conf_el candidate = access(depth, pos + i);

        if (candidate.empty()) {
            MEASURE_ONLY(meas.lookup_miss_reached_empty++);
            break;
        }

        if (candidate.match(h)) {
            MEASURE_ONLY(meas.lookup_hit++);
            return candidate.win();
        }
    }

    return victory::uncertain;
}


// We only insert a binconf if either adversary or algorithm wins.
void state_layered_precache::insert_binconf(binconf *bc, victory win) {
    conf_el e;
    uint64_t h = bc->statehash();
    e.set(h, static_cast<bool>(win));
    // insert(e, h);
    conf_el candidate;
    int depth = bc->itemcount();
    uint64_t pos;
    if (SMALL_CACHE(depth)) {
        pos = trim_small(h);
    } else {
        pos = trim_large(h);
    }

    // If this is the first element with this prefix stored, then we can just store at position zero without
    // reading.
    if (!precache[depth][trim_precache(h)]) {
        store(depth, pos, e);
        MEASURE_ONLY(meas.insert_into_empty++);
        precache[depth][trim_precache(h)] = true;
        return;
    }

    precache[depth][trim_precache(h)] = true;

    // However, even if the precache says something is there, it might not be at the full position, only in the
    // prefix region. Hence, we check i = 0 in the next loop as well.
    for (int i = 0; i < static_cast<int>(LINPROBE_LIMIT); i++) {
        candidate = access(depth, pos + i);
        if (candidate.empty()) {
            MEASURE_ONLY(meas.insert_into_empty++);
            store(depth, pos + i, e);
            // return INSERTED;
            return;
        } else if (candidate.match(h)) {
            MEASURE_ONLY(meas.insert_duplicate++);
            return;
        }
    }

    store(depth, pos + (rand() % LINPROBE_LIMIT), e);
    MEASURE_ONLY(meas.insert_randomly++);
}

