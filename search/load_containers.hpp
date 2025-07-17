#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include "binomial_index.hpp"

template <int BINCAP, int MAX_LOAD>
class array_loads {
public:
    std::array<int, BINCAP + 1> loads = {};

};


template <int BINCAP, int MAX_LOAD>
class packed_shorts {
public:
};



template <int BINCAP, int MAX_LOAD>
class packed_chars_eight {
public:
    static_assert(BINCAP <= 8 && MAX_LOAD <= std::numeric_limits<char8_t>::max(),
        "Packed chars (8) can only be used when capacity is at most 8.");
    uint64_t word_;
    // Zero-initialize all bytes
    constexpr packed_chars_eight() noexcept : word_(0) {}
    inline void clear() noexcept {
        word_ = 0;
    }

    // Read byte i (0..7)
    inline unsigned char operator[](std::size_t pos) const noexcept {
        // Hack -- be able to take 1..8 instead of 0..7.
        --pos;
        // shift = pos * 8
        return static_cast<unsigned char>(word_ >> (pos << 3));
    }


    bool operator==(const packed_chars_eight& rhs) const {
        return rhs.word_ == word_;
    }

    // Store 'val' into byte pos
    inline void store(std::size_t pos, unsigned char val) noexcept {
        const uint64_t shift = pos << 3;
        const uint64_t mask  = uint64_t(0xFF) << shift;        // bits to replace
        word_ = (word_ & ~mask) | (uint64_t(val) << shift);
    }

    // Add 'val' to byte pos (wraps mod 256)
    inline void add_to(std::size_t pos, unsigned char val) noexcept {
        const uint64_t shift = pos << 3;
        const uint64_t mask  = uint64_t(0xFF) << shift;
        const uint64_t inc   = uint64_t(val) << shift;
        // add, mask off any carry into neighbors, then re-or in untouched bytes
        word_ = ((word_ + inc) & mask) | (word_ & ~mask);
    }

    // Subtract 'val' from byte pos (wraps mod 256)
    inline void remove_from(std::size_t pos, unsigned char val) noexcept {
        const uint64_t shift = pos << 3;
        const uint64_t mask  = uint64_t(0xFF) << shift;
        const uint64_t dec   = uint64_t(val) << shift;
        word_ = ((word_ - dec) & mask) | (word_ & ~mask);
    }

    // Pre-increment byte pos, return new value
    inline unsigned char plusplus(std::size_t pos) noexcept {
        const uint64_t shift = pos << 3;
        const uint64_t mask  = uint64_t(0xFF) << shift;
        const uint64_t one   = uint64_t(1) << shift;
        word_ = ((word_ + one) & mask) | (word_ & ~mask);
        return static_cast<unsigned char>(word_ >> shift);
    }

    // Pre-decrement byte pos, return new value
    inline unsigned char minusminus(std::size_t pos) noexcept {
        const uint64_t shift = pos << 3;
        const uint64_t mask  = uint64_t(0xFF) << shift;
        const uint64_t one   = uint64_t(1) << shift;
        word_ = ((word_ - one) & mask) | (word_ & ~mask);
        return static_cast<unsigned char>(word_ >> shift);
    }


    inline std::size_t one_increased(std::size_t pos, unsigned char value) noexcept {
        --pos;
        uint64_t w    = word_;
        unsigned old  = (w >> (pos<<3)) & 0xFFu;
        unsigned newv = static_cast<unsigned char>(old + value);  // mod-256

        if (pos == 0) {
            // only byte 0: clear it, insert newv, done
            const uint64_t mask = uint64_t(0xFF);
            w = (w & ~mask) | uint64_t(newv);
            word_ = w;
            return 1;
        }

        // --- find t exactly as before ---
        __m128i vec_w   = _mm_cvtsi64_si128(static_cast<long long>(w));
        __m128i vec_new = _mm_set1_epi8(char(newv));
        __m128i bias    = _mm_set1_epi8(char(0x80));
        __m128i cmp     = _mm_cmpgt_epi8(
                              _mm_xor_si128(vec_new, bias),
                              _mm_xor_si128(vec_w,   bias)
                          );
        unsigned mask   = _mm_movemask_epi8(cmp) & ((1u << pos) - 1);
        std::size_t t   = mask ? __builtin_ctz(mask) : pos;

        // --- build shift & clear masks ---
        uint64_t shifted = 0, region = 0;
        if (t < pos) {
            // slide block [t..pos-1] one byte toward MSB
            int t8   = int(t<<3);
            int len  = int(pos - t);
            uint64_t low   = ~0ULL << t8;
            uint64_t high  = ~0ULL >> ((8 - int(pos))<<3);
            uint64_t block = w & (low & high);
            shifted = block << 8;
            // region = bits [t..pos] to clear
            region = (~0ULL >> (((7 - len)<<3))) << t8;
        } else {
            // no sliding needed, just clear byte t==pos
            region = uint64_t(0xFF) << (pos<<3);
        }

        // --- clear region, OR in shifted block + newv ---
        w = (w & ~region)
          | shifted
          | (uint64_t(newv) << (t<<3));

        word_ = w;
        return t+1;
    }

    // Decrease x[pos] by `value`, then re-insert it so x[0] ≥ x[1] ≥ … ≥ x[7]
    // Returns the new index t where the diminished value landed.
    inline std::size_t one_decreased(std::size_t pos, unsigned char value) noexcept {
        // Hack -- be able to take 1..8 instead of 0..7.
        --pos;

        uint64_t w    = word_;
        unsigned old  = static_cast<unsigned>(w >> (pos<<3)) & 0xFFu;
        unsigned newv = static_cast<unsigned char>(old - value);  // mod-256 wrap

        // if at far right already, just store and return pos
        if (pos == 7) {
            const uint64_t mask = uint64_t(0xFF) << (pos<<3);
            w = (w & ~mask) | (uint64_t(newv) << (pos<<3));
            word_ = w;
            // return 7;
            return 8;
        }

        // SIMD compare: want bits i>pos where x[i] > newv
        __m128i vec_w   = _mm_cvtsi64_si128((long long)w);
        __m128i vec_new = _mm_set1_epi8(char(newv));
        __m128i bias    = _mm_set1_epi8(char(0x80));
        // signed compare after bias ⇒ unsigned compare
        __m128i cmp = _mm_cmpgt_epi8(
                          _mm_xor_si128(vec_w,   bias),
                          _mm_xor_si128(vec_new, bias)
                      );
        unsigned mask = _mm_movemask_epi8(cmp)
                        & ~((1u << (pos+1)) - 1);   // keep only bits ≥ pos+1

        // find highest bit i in mask ⇒ insertion index t; else t = pos
        std::size_t t = mask
            ? (31u - __builtin_clz(mask))
            : pos;

        // build masks for the block [pos+1..t] and the full region [pos..t]
        const int p8 = int(pos+1)<<3;
        const int t8 = int(t)<<3;
        uint64_t block_mask  = (~0ULL << p8) & (~0ULL >> ((7 - int(t))<<3));
        uint64_t region_mask = (~0ULL << (pos<<3)) & (~0ULL >> ((7 - int(t))<<3));

        // shift that block one byte toward LSB (right in little-endian)
        uint64_t block   = w & block_mask;
        uint64_t shifted = block >> 8;

        // clear old [pos..t], insert shifted block and then newv at byte t
        w = (w & ~region_mask) | shifted | (uint64_t(newv) << t8);
        word_ = w;
        // return t;
        return t+1;
    }
};

/* 16×u8 packed into one 128-bit register */
template <int BINCAP, int MAX_LOAD> class packed_chars_sixteen {
public:
    static_assert(BINCAP <= 16 && MAX_LOAD <= std::numeric_limits<char8_t>::max(),
        "Packed chars (16) can only be used with at most 16 bins.");
    unsigned __int128 word_{};               // storage (16 × 8 bits)

    bool operator==(const packed_chars_sixteen& rhs) const {
        return rhs.word_ == word_;
    }
    /* -------- array-style access -------- */
    // inline proxy operator[](std::size_t i)       noexcept { return proxy{word_, i * 8}; }
    // inline unsigned char operator[](std::size_t i) const noexcept {
    //     return static_cast<unsigned char>((word_ >> (i * 8)) & 0xFFu);
    // }

    // Partial surrender for packing.
    // inline unsigned char operator[](std::size_t i) const noexcept {
    //     return reinterpret_cast<const unsigned char*>(&word_)[i];
    // }

    // Hack. Until now, we have kept the first position empty, which means that a load of [4 3 2] would be
    // stored as [0 4 3 2], but now it hurts us.
    // To band-aid this, the operator[] will return a value from a position one level below.

    inline unsigned char operator[](std::size_t i) const noexcept {
        return reinterpret_cast<const unsigned char*>(&word_)[i-1];
    }

    // inline unsigned char operator[](std::size_t k) const noexcept {
    //     unsigned idx = k - 1;                 // 0 … 15
    //
    //     uint64_t lo  = static_cast<uint64_t>(word_);          // low  64 bits
    //     uint64_t hi  = static_cast<uint64_t>(word_ >> 64);    // high 64 bits
    //
    //     // idx >> 3 is 0 for bytes 0–7, 1 for bytes 8–15.
    //     uint64_t word = (idx >> 3) ? hi : lo;             // branch-free choose half
    //
    //     unsigned shift = (idx & 7) << 3;                  // (idx % 8) * 8
    //     return static_cast<unsigned char>(word >> shift);       // isolate the wanted byte
    // }

    // static inline unsigned char fast_extract(uint64_t part,
    //                                      unsigned offset) noexcept {
    //     // rcx = (offset << 8) | 8   –– low 8 bits = width, high = start-bit
    //     return static_cast<unsigned char>(
    //         _bextr_u64(part, offset * 8, 8));         // width = 8 bits
    // }
    //
    // inline unsigned char operator[](std::size_t i) const noexcept {
    //     const uint64_t lo = static_cast<uint64_t>( word_        );
    //     const uint64_t hi = static_cast<uint64_t>( word_ >> 64 );
    //
    //     // choose the right half without a branch (cmov emits 1 µop)
    //     const uint64_t part = (i < 8) ? lo : hi;
    //
    //     return fast_extract(part, static_cast<unsigned>(i & 7));
    // }

    inline void clear() {
            word_ = static_cast<unsigned __int128>(0);
    }

    // inline void store(std::size_t i, unsigned char v) noexcept
    // {
    //     const unsigned __int128 shift = static_cast<unsigned __int128>(i) * 8;   // bit offset
    //     const unsigned __int128 mask  = static_cast<unsigned __int128>(0xFF) << shift;
    //
    //     word_ = (word_ & ~mask)                         // clear the target byte
    //           | (static_cast<unsigned __int128>(v) << shift);  // insert new value
    // }

    //------------------------------------------------------------------
    // helpers: raw byte access that the strict-aliasing rules allow
    //------------------------------------------------------------------
    static inline unsigned char  get_byte(const unsigned __int128& w,
                                          unsigned idx)             noexcept
    {
        return reinterpret_cast<const unsigned char*>(&w)[idx];
    }
    static inline void set_byte(unsigned __int128& w,
                                unsigned idx, unsigned char v)      noexcept
    {
        reinterpret_cast<unsigned char*>(&w)[idx] = v;
    }

    //------------------------------------------------------------------
    // SIMD/BMI1: locate first element that is smaller than key
    //------------------------------------------------------------------
    static inline unsigned find_insert_pos_simd(const unsigned __int128& w,
                                                unsigned               pos,
                                                unsigned char          key) noexcept
    {
#if defined(__SSSE3__)                       // we have pshufb + movemask
        const __m128i v     = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(&w));

        // XOR by 0x80 to turn unsigned-byte order into signed-byte order,
        // then signed-compare < key
        const __m128i off   = _mm_set1_epi8(static_cast<char>(0x80));
        const __m128i vecx  = _mm_xor_si128(v,    off);
        const __m128i keyx  = _mm_set1_epi8(static_cast<char>(key ^ 0x80));

        const __m128i cmp   = _mm_cmplt_epi8(vecx, keyx);
        unsigned mask       = static_cast<unsigned>(_mm_movemask_epi8(cmp));

        mask &= (1u << pos) - 1u;           // ignore bytes right of pos
        return mask ? __builtin_ctz(mask)   // first “smaller” element
                    : pos;                  // none – stay where we were
#else                                        // portable scalar fallback
#warning "Portable version will be used -- slow."
        unsigned i = pos;
        for (; i > 0 && get_byte(w, i - 1) < key; --i) ;
        return i;
#endif
    }

    // SIMD/BMI1: return the final index *dest* where the smaller byte must land
    //            (i.e. one position before the first value < new_val)
    //  - input  : w   = packed bytes
    //             pos = original position of the byte we just shrank
    //             key = new_val  (bytes[pos] - value)
    //  - output : dest = pos .. 15    (always ≥ pos)
    static inline unsigned find_dest_pos_simd_right(const unsigned __int128& w,
                                                    unsigned               pos,
                                                    unsigned char          key) noexcept
    {
#if defined(__SSSE3__)
        const __m128i v    = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(&w));
        const __m128i off  = _mm_set1_epi8(static_cast<char>(0x80));
        const __m128i vecx = _mm_xor_si128(v, off);
        const __m128i kx   = _mm_set1_epi8(static_cast<char>(key ^ 0x80));

        // vecx < key  ?
        const __m128i cmp_lt = _mm_cmplt_epi8(vecx, kx);
        unsigned mask_lt     = static_cast<unsigned>(_mm_movemask_epi8(cmp_lt));

        // keep only bytes *to the right* of pos
        mask_lt &= ~((1u << (pos + 1)) - 1u);

        if (!mask_lt)                       // no smaller value to the right
            return 15u;                     // …so new_val goes to the end

        unsigned j = __builtin_ctz(mask_lt);  // first byte < new_val
        return j - 1u;                        // insert *before* it
#else
        unsigned i = pos;
        while (i < 15 && get_byte(w, i + 1) > key) ++i;
        return i;                           // same semantics as the SIMD path
#endif
    }

public:
    // -----------------------------------------------------------------
    //  one_increased :  add ‘value’ to element @pos, keep descending order
    // -----------------------------------------------------------------
    inline std::size_t one_increased(std::size_t  pos,
                                     unsigned char value) noexcept {
        // Hack. This is again due to the fact that until now, loads were stored in a BINS+1 sized array.
        // This likely needs to go away soon.
        --pos;
        // 1) compute the new byte
        unsigned char* bytes =
            reinterpret_cast<unsigned char*>(&word_);

        unsigned char new_val = static_cast<unsigned char>(bytes[pos] + value);

        // 2) find the place where it now belongs (scan leftwards)
        const unsigned new_pos =
            find_insert_pos_simd(word_, static_cast<unsigned>(pos), new_val);

        // 3) nothing moved?  — just overwrite and quit
        if (new_pos == pos) {
            bytes[pos] = new_val;
            // Again, hack.
            return pos+1;
        }

        // 4) shift the run [new_pos .. pos-1] one step right
        for (unsigned i = pos; i > new_pos; --i) {
            bytes[i] = bytes[i - 1];
        }

        // 5) drop the new value in its slot
        bytes[new_pos] = new_val;
        // Again, a hack to reindex back to the 1 <= new_pos <= BINS range.
        return new_pos+1;
    }

    // -----------------------------------------------------------------
    //  one_decreased : subtract ‘value’, keep descending order (moves RIGHT)
    // -----------------------------------------------------------------
    inline std::size_t one_decreased(std::size_t  pos,
                                     unsigned char value) noexcept {
        // Hack. This is again due to the fact that until now, loads were stored in a BINS+1 sized array.
        // This likely needs to go away soon.
        --pos;

        unsigned char* bytes =
            reinterpret_cast<unsigned char*>(&word_);

        const unsigned char new_val =
            static_cast<unsigned char>(bytes[pos] - value);


        const unsigned new_pos =
            find_dest_pos_simd_right(word_, static_cast<unsigned>(pos),
                                       new_val);

        if (new_pos == pos) {                // stays in place
            bytes[pos] = new_val;
            // Again, hack.
            return pos+1;
        }

        // shift run [pos+1 .. new_pos] left by one
        for (unsigned i = pos; i < new_pos; ++i) {
            bytes[i] = bytes[i + 1];
        }

        bytes[new_pos] = new_val;
        // Again, a hack to reindex back to the 1 <= new_pos <= BINS range.
        return new_pos+1;
    }



    // ---------------------------------------------------------------------
// Helper:  where will the larger byte land?  (descending order)
//
// Returns dest ∈ [0, bin] such that
//      loads[dest-1] ≥ newload > loads[dest]
// ---------------------------------------------------------------------
static inline unsigned find_dest_left(const unsigned __int128& w,
                                      unsigned               bin,
                                      unsigned char          newload) noexcept
{
#if defined(__SSSE3__)
    // load the 16 packed bytes
    const __m128i v    = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&w));

    // unsigned→signed trick: x ^ 0x80 preserves order
    const __m128i off  = _mm_set1_epi8(char(0x80));
    const __m128i vecx = _mm_xor_si128(v, off);
    const __m128i keyx = _mm_set1_epi8(char(newload ^ 0x80));

    // vecx < keyx ?
    const __m128i cmp  = _mm_cmplt_epi8(vecx, keyx);
    unsigned mask      = unsigned(_mm_movemask_epi8(cmp));

    // consider only bytes *left* of the current bin
    mask &= (1u << bin) - 1u;

    return mask ? __builtin_ctz(mask)        // first smaller element
                : bin;                       // stays in place
#else
    const unsigned char* loads =
        reinterpret_cast<const unsigned char*>(&w);
    unsigned cur = bin;
    while (cur > 0 && loads[cur - 1] < newload) --cur;
    return cur;
#endif
}

// ---------------------------------------------------------------------
//  virtual_index  (item > 0 only)
// ---------------------------------------------------------------------
inline index_t virtual_index(index_t old_index, unsigned char item, std::size_t bin) const noexcept
{
    const unsigned char* loads =
        reinterpret_cast<const unsigned char*>(&word_);

    const unsigned char oldload = loads[bin];
    const unsigned char newload = static_cast<unsigned char>(oldload + item);

    // 1) locate the destination bin (cannot be right of ‘bin’)
    const unsigned dest = find_dest_left(word_, unsigned(bin), newload);
    // fprintf(stderr, "The new load %" PRIu8 "will move to %u.\n", newload, dest);

    // 2) fast path: value stays where it is
    if (dest == bin) {
        return old_index + binoms_gl[(bin) * R + newload] - binoms_gl[(bin) * R + oldload];
    }

    index_t delta = 0;
    for (unsigned r = bin; r > dest; --r) {
        delta +=  binoms_gl[(r) * R + loads[r-1]] - binoms_gl[(r) * R + loads[r]];
    }
    // final replacement: insert newload into row dest-1
    delta += binoms_gl[dest * R + newload] - binoms_gl[dest * R + loads[dest]];


    return old_index + delta;
}

    // Reinterpret cast-type operations.

    inline void store(std::size_t pos, unsigned char val) noexcept {
        reinterpret_cast<unsigned char*>(&word_)[pos] = val;
    }

    inline void add_to(std::size_t pos, unsigned char val) noexcept {
        reinterpret_cast<unsigned char*>(&word_)[pos] += val;
    }

    inline void remove_from(std::size_t pos, unsigned char val) noexcept {
        reinterpret_cast<unsigned char*>(&word_)[pos] -= val;
    }

    inline unsigned char plusplus(std::size_t pos) noexcept {
        unsigned char* p = reinterpret_cast<unsigned char*>(&word_);
        return ++p[pos];
    }

    inline unsigned char minusminus(std::size_t pos) noexcept {
        unsigned char* p = reinterpret_cast<unsigned char*>(&word_);
        return --p[pos];
    }
};