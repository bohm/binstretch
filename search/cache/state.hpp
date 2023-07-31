#pragma once

#include <cstdio>

#include "../common.hpp"
#include "../hash.hpp"

// Implementations of specific caches, using the interface defined in cache_generic.hpp.


class conf_el {
public:
    uint64_t _data;

    inline void set(uint64_t hash, uint64_t val) {
        // assert(val == 0 || val == 1);
        _data = (zero_last_bit(hash) | val);
    }

    inline bool value() const {
        return get_last_bit(_data);
    }

    inline uint64_t hash() const {
        return zero_last_bit(_data);
    }

    inline bool match(const uint64_t &hash) const {
        return (zero_last_bit(_data) == zero_last_bit(hash));
    }

    inline bool removed() const {
        return false; // Currently not implemented.
    }

    inline bool empty() const {
        return _data == 0;
    }

    inline void erase() {
        _data = 0;
    }

    int depth() const {
        return 0;
    }

    static const conf_el ZERO;
};

const conf_el conf_el::ZERO{0};


class state_cache // : public cache<conf_el, uint64_t, int>
{
public:
    std::atomic<conf_el> *ht;
    uint64_t htsize;
    int logsize;
    cache_measurements meas;

    void atomic_init_point(uint64_t point) {
        std::atomic_init(&ht[point], conf_el::ZERO);
    }

    void parallel_init_segment(uint64_t start, uint64_t end, uint64_t size) {
        for (uint64_t i = start; i < std::min(end, size); i++) {
            atomic_init_point(i);
        }
    }

    state_cache(uint64_t logbytes, int threads, std::string descriptor = "") {
        assert(logbytes >= 0 && logbytes <= 64);

        uint64_t bytes = two_to(logbytes);
        const uint64_t megabyte = 1024 * 1024;

        htsize = power_of_two_below(bytes / sizeof(conf_el));
        logsize = quicklog(htsize);
        print_if<PROGRESS>(
                "Given %llu logbytes (%llu MBs) and el. size %zu, creating %s state cache (64-bit hashes) to %llu els (logsize %llu).\n",
                logbytes, bytes / megabyte, sizeof(conf_el), descriptor.c_str(), htsize, logsize);


        ht = new std::atomic<conf_el>[htsize];
        assert(ht != NULL);

        uint64_t segment = htsize / threads;
        uint64_t start = 0;
        uint64_t end = std::min(htsize, segment);

        std::vector<std::thread> th;
        for (int w = 0; w < threads; w++) {
            th.push_back(std::thread(&state_cache::parallel_init_segment, this, start, end, htsize));
            start += segment;
            end += segment;
            start = std::min(start, htsize);
            end = std::min(end, htsize);
        }

        for (int w = 0; w < threads; w++) {
            th[w].join();
        }
    }

    ~state_cache() {
        delete ht;
    }

    conf_el access(uint64_t pos) {
        return ht[pos].load(std::memory_order_relaxed);
    }

    void store(uint64_t pos, const conf_el &e) {
        ht[pos].store(e, std::memory_order_relaxed);
    }

    uint64_t size() {
        return htsize;
    }

    uint64_t trim(uint64_t ha) {
        return logpart(ha, logsize);
    }

    void analysis();

    std::pair<bool, bool> lookup(uint64_t h);

    void insert(conf_el e, uint64_t h);


    // Functions for clearing part of entirety of the cache.

    void clear_cache_segment(uint64_t start, uint64_t end) {
        for (uint64_t i = start; i < std::min(end, htsize); i++) {
            ht[i].store(conf_el::ZERO);
        }
    }

    void clear_cache(int threads) {
        uint64_t segment = htsize / threads;
        uint64_t start = 0;
        uint64_t end = std::min(htsize, segment);

        std::vector<std::thread> th;
        for (int w = 0; w < threads; w++) {
            th.push_back(std::thread(&state_cache::clear_cache_segment, this, start, end));
            start += segment;
            end += segment;
            start = std::min(start, htsize);
            end = std::min(end, htsize);
        }

        for (int w = 0; w < threads; w++) {
            th[w].join();
        }
    }

    void clear_ones_segment(uint64_t start, uint64_t end) {
        for (uint64_t i = start; i < std::min(end, htsize); i++) {
            conf_el field = ht[i];
            if (!field.empty()) {
                int last_bit = field.value();
                if (last_bit != 0) {
                    ht[i].store(conf_el::ZERO);
                }
            }
        }
    }

    void clear_cache_of_infeasible(int threads) {
        uint64_t segment = htsize / threads;
        uint64_t start = 0;
        uint64_t end = std::min(htsize, segment);

        std::vector<std::thread> th;
        for (int w = 0; w < threads; w++) {
            th.push_back(std::thread(&state_cache::clear_ones_segment, this, start, end));
            start += segment;
            end += segment;
            start = std::min(start, htsize);
            end = std::min(end, htsize);
        }

        for (int w = 0; w < threads; w++) {
            th[w].join();
        }
    }
};

std::pair<bool, bool> state_cache::lookup(uint64_t h) {
    conf_el candidate;
    uint64_t pos = trim(h);
    // Use linear probing to check for the hashed value.
    for (int i = 0; i < LINPROBE_LIMIT; i++) {
        assert(pos + i < size());
        candidate = access(pos + i);

        if (candidate.empty()) {
            MEASURE_ONLY(meas.lookup_miss_reached_empty++);
            break;
        }

        if (candidate.match(h)) {
            MEASURE_ONLY(meas.lookup_hit++);
            return std::make_pair(true, candidate.value());
        }

        // bounds check (the second case is so that measurements are okay)
        if (pos + i + 1 == size() || i == LINPROBE_LIMIT - 1) {
            MEASURE_ONLY(meas.lookup_miss_full++);
            break;
        }
    }

    return std::make_pair(false, false);
}

void state_cache::insert(conf_el e, uint64_t h) {
    conf_el candidate;
    uint64_t pos = trim(h);

    int limit = LINPROBE_LIMIT;
    if (pos + limit > size()) {
        limit = std::max((uint64_t) 0, size() - pos);
    }

    for (int i = 0; i < limit; i++) {
        candidate = access(pos + i);
        if (candidate.empty() || candidate.removed()) {
            MEASURE_ONLY(meas.insert_into_empty++);
            store(pos + i, e);
            // return INSERTED;
            return;
        } else if (candidate.match(h)) {
            MEASURE_ONLY(meas.insert_duplicate++);
            return;
        }
    }

    store(pos + (rand() % limit), e);
    MEASURE_ONLY(meas.insert_randomly++);
    return;
}

void state_cache::analysis() {
    for (uint64_t i = 0; i < htsize; i++) {
        if (ht[i].load().empty()) {
            meas.empty_positions++;
        } else {
            meas.filled_positions++;
        }
    }
}



// Global pointer to the adversary position cache.

state_cache *adv_cache = NULL;

// Algorithmic positional cache is less useful in the following sense:
// unlike the adversary position cache, every algorithmic vertex has
// indegree 1 -- you can only reach it from a very specific position
// with a unique item to be sent.

// It is still possible to visit an algorithmic vertex twice, but
// adversarial vertices have larger indegrees and the cache makes thus
// much more sense.

// state_cache *alg_cache = NULL;

void adv_cache_encache_adv_win(const binconf *d) {
    uint64_t bchash = d->statehash();
    conf_el new_item;
    new_item.set(bchash, 0);
    adv_cache->insert(new_item, bchash);
}

void adv_cache_encache_alg_win(const binconf *d) {
    uint64_t bchash = d->statehash();
    conf_el new_item;
    new_item.set(bchash, 1);
    adv_cache->insert(new_item, bchash);
}
