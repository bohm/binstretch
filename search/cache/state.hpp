#pragma once

#include <cstdio>

#include "../common.hpp"
#include "../hash.hpp"

#include "net/local/threadsafe_printer.hpp"
// Implementations of specific caches, using the interface defined in cache_generic.hpp.

// Note: With the recent shift from the index being the first k bits to the last k bits,
// it is true that a single position does not contain its full hash in the _data field;
// this is because the last bit is used for the boolean victory value.

// This should not be an issue, because match(right hash) == true.
class conf_el {
public:
    uint64_t _data;

    conf_el() {_data = 0;}
    explicit conf_el(uint64_t val) : _data(val) {}

    inline void set(uint64_t hash, uint64_t val) {
        // assert(val == 0 || val == 1);
        _data = (zero_last_bit(hash) | val);
    }

    inline bool value() const {
        return get_last_bit(_data);
    }

    inline victory win() const {
        return static_cast<enum victory>(get_last_bit(_data));
    }

    inline bool match(const uint64_t hash) const {
        return (_data ^ hash) <= 1;
        //return (zero_last_bit(_data) == zero_last_bit(hash));
    }

    /*
    inline bool removed() const {
        return false; // Currently not implemented.
    }
    */

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
    std::atomic<uint64_t> *ht;
    uint64_t htsize;
    int logsize;
    cache_measurements meas;

    void atomic_init_point(uint64_t point) {
        std::atomic_init(&ht[point], 0);
    }

    void parallel_init_segment(uint64_t start, uint64_t end, uint64_t size) {
        for (uint64_t i = start; i < std::min(end, size); i++) {
            atomic_init_point(i);
        }
    }

    state_cache(uint64_t logbytes, int threads, std::string descriptor = "") {
        assert(logbytes <= 64);

        uint64_t bytes = two_to(logbytes);
        const uint64_t megabyte = 1024 * 1024;

        htsize = power_of_two_below(bytes / sizeof(conf_el));
        logsize = quicklog(htsize);
        print_if<PROGRESS>(
                "Given %llu logbytes (%llu MBs) and el. size %zu, creating %s state cache (64-bit hashes) to %llu els (logsize %llu).\n",
                logbytes, bytes / megabyte, sizeof(conf_el), descriptor.c_str(), htsize, logsize);


        // We allocate not 2^{k} sized cache (which makes sense because of the indexing) but we add LINPROBE_LIMIT
        // extra elements for linear probing to work for the last LINPROBE_LIMIT elements of the cache itself.
        ht = new std::atomic<uint64_t>[htsize+LINPROBE_LIMIT];
        assert(ht != nullptr);

        uint64_t segment = htsize / threads;
        uint64_t start = 0;
        uint64_t end = std::min(htsize, segment);

        std::vector<std::thread> th;
        for (int w = 0; w < threads; w++) {
            th.emplace_back(&state_cache::parallel_init_segment, this, start, end, htsize);
            start += segment;
            end += segment;
            start = std::min(start, htsize);
            end = std::min(end, htsize);
        }

        for (int w = 0; w < threads; w++) {
            th[w].join();
        }

        // Deal with the tail.
        for (uint64_t tail = htsize; tail < htsize+LINPROBE_LIMIT; tail++) {
            atomic_init_point(tail);
        }
    }

    ~state_cache() {
        delete ht;
    }

    inline conf_el access(uint64_t pos) const {
        return conf_el(ht[pos].load(std::memory_order_relaxed));
    }

    void store(uint64_t pos, const conf_el &e) {
        ht[pos].store(e._data, std::memory_order_relaxed);
    }

    uint64_t size() const {
        return htsize;
    }

    uint64_t trim(uint64_t ha) const {
        return last_k_bits(ha, logsize);
    }

    // Intentionally does nothing. Only relevant for state_analysis_cache.
    void report() {

    }

    void analysis();

    // victory lookup(uint64_t h);
    victory lookup_virtual(binconf *bc, int item, unsigned char bin);

    // void insert(conf_el e, uint64_t h);
    void insert_binconf(binconf *bc, victory algorithm_victory);

    // Functions for clearing part of entirety of the cache.

    void clear_cache_segment(uint64_t start, uint64_t end) {
        for (uint64_t i = start; i < std::min(end, htsize); i++) {
            ht[i].store(0);
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

        // Deal with the tail.
        for (uint64_t tail = htsize; tail < htsize+LINPROBE_LIMIT; tail++) {
            ht[tail].store(0);
        }
    }

    void clear_ones_segment(uint64_t start, uint64_t end) {
        for (uint64_t i = start; i < std::min(end, htsize+LINPROBE_LIMIT); i++) {
            conf_el field = access(i);
            if (!field.empty()) {
                int last_bit = field.value();
                if (last_bit != 0) {
                    ht[i].store(0);
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
            th.emplace_back(&state_cache::clear_ones_segment, this, start, end);
            start += segment;
            end += segment;
            start = std::min(start, htsize);
            end = std::min(end, htsize);
        }

        for (int w = 0; w < threads; w++) {
            th[w].join();
        }

        clear_ones_segment(htsize, htsize+LINPROBE_LIMIT);
    }
};

// inline victory state_cache::lookup(uint64_t h) {
//     uint64_t pos = trim(h);
//     // Use linear probing to check for the hashed value.
//     // uint64_t limit = std::min(size()-pos, LINPROBE_LIMIT);
//     for (uint64_t i = 0; i < LINPROBE_LIMIT; i++) {
//         // assert(pos + i < size());
//         conf_el candidate = access(pos + i);
//
//         if (candidate.empty()) {
//             MEASURE_ONLY(meas.lookup_miss_reached_empty++);
//             break;
//         }
//
//         if (candidate.match(h)) {
//             MEASURE_ONLY(meas.lookup_hit++);
//             return {true, candidate.value()};
//         }
//
//         // bounds check (the second case is so that measurements are okay)
//         // if (pos + i + 1 == size() || i == LINPROBE_LIMIT - 1) {
//         //     MEASURE_ONLY(meas.lookup_miss_full++);
//         //    break;
//         // }
//     }
//
//     return {false, false};
// }

// Some explanations here. The API is cleaner with just calling lookup() directly,
// but in order to be able to swap out this good cache for a cache that can do
// deep measurements, we need to be more general in what is being passed.

inline victory state_cache::lookup_virtual(binconf *bc, int item, unsigned char bin) {
    uint64_t h = bc->virtual_hash_with_low(item, bin);
    uint64_t pos = trim(h);

    // Use linear probing to check for the hashed value.
    for (uint64_t i = 0; i < LINPROBE_LIMIT; i++) {
        // assert(pos + i < size());
        conf_el candidate = access(pos + i);

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
void state_cache::insert_binconf(binconf *bc, victory win) {
    conf_el e;
    uint64_t h = bc->statehash();
    e.set(h, static_cast<bool>(win));
    // insert(e, h);
    conf_el candidate;
    uint64_t pos = trim(h);


    for (int i = 0; i < static_cast<int>(LINPROBE_LIMIT); i++) {
        candidate = access(pos + i);
        if (candidate.empty()) {
            MEASURE_ONLY(meas.insert_into_empty++);
            store(pos + i, e);
            // return INSERTED;
            return;
        } else if (candidate.match(h)) {
            MEASURE_ONLY(meas.insert_duplicate++);
            return;
        }
    }

    store(pos + (rand() % LINPROBE_LIMIT), e);
    MEASURE_ONLY(meas.insert_randomly++);
}

void state_cache::analysis() {
    for (uint64_t i = 0; i < htsize; i++) {
        if (ht[i].load() == 0) {
            meas.empty_positions++;
        } else {
            meas.filled_positions++;
        }
    }
}



// Algorithmic positional cache is less useful in the following sense:
// unlike the adversary position cache, every algorithmic vertex has
// indegree 1 -- you can only reach it from a very specific position
// with a unique item to be sent.

// It is still possible to visit an algorithmic vertex twice, but
// adversarial vertices have larger indegrees and the cache makes thus
// much more sense.

// void adv_cache_encache_adv_win(state_cache *cache, const binconf *d) {
//
//     uint64_t bchash = d->statehash();
//     conf_el new_item;
//     new_item.set(bchash, 0);
//     // Deep debug. Remove as soon as possible.
//     // adv_win_state_file.print_with_binconf(d,"Storing statehash (%" PRIu64 ") as adv-winning for binconf ", bchash);
//     cache->insert(new_item, bchash);
// }
//
// void adv_cache_encache_alg_win(state_cache *cache, const binconf *d) {
//     uint64_t bchash = d->statehash();
//     conf_el new_item;
//     new_item.set(bchash, 1);
//     // Deep debug. Remove as soon as possible.
//     // alg_win_state_file.print_with_binconf(d, "Storing statehash (%" PRIu64 ") as alg-winning for binconf ", bchash);
//     cache->insert(new_item, bchash);
// }
