#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

/* 16×u8 packed into one 128-bit register */
class packed_loadconf {
public:
    unsigned __int128 word_{};               // storage (16 × 8 bits)

    static constexpr std::size_t kSize = 16; // matches std::array::size

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