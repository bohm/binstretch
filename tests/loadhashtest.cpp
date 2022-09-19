// Testing various hashing optimizations (for loadhash). 
// We hardcode the constants to make this file unbreakable with search changes.

#include <cstdio>
#include <cassert>
#include <cinttypes>

#include <random>
#include <array>
#include <chrono>

typedef int16_t bin_int;

// I do not think those are necessary to be duplicated, this is some historical
// error.
const int IBINS = 10;
const int IR = 19;
const int IS = 14;

const bin_int S = (bin_int) IS;
const bin_int R = (bin_int) IR;
const bin_int BINS = (bin_int) IBINS;

// The size of one block in the blocksize experiment.
const int ZOBRIST_LOAD_BLOCKSIZE = 5;
const int ZOBRIST_LOAD_BLOCKS = (IBINS-1)/ZOBRIST_LOAD_BLOCKSIZE + 1;
const int ZOBRIST_LAST_BLOCKSIZE = (IBINS-1) % ZOBRIST_LOAD_BLOCKSIZE + 1;

const bool DEBUG = false;

const int TESTS = 50000000;

// Mersenne twister.
std::mt19937_64 gen(12345);

uint64_t *Zl; // Zobrist table for loads
uint64_t **Zlbig;

// A compile-time power computation.
// Source: https://stackoverflow.com/a/27271374

template<typename T>
constexpr T sqr(T a) {
    return a * a;
}

template<typename T>
constexpr T power(T a, std::size_t n) {
    return n == 0 ? 1 : sqr(power(a, n / 2)) * (n % 2 == 0 ?  1 : a);
}



uint64_t rand_64bit()
{
    uint64_t r = gen();
    return r;
}

bin_int rand_load()
{
    uint64_t r = gen();
    bin_int rb = (bin_int) ( r % (R+1));
    return rb;
}


void print_standard_block(std::array<bin_int, ZOBRIST_LOAD_BLOCKSIZE> arr)
{
    bool first = true;
    for (int i = 0; i < ZOBRIST_LOAD_BLOCKSIZE; i++)
    {
	if(first)
	{
	    fprintf(stderr, "[%d", arr[i]);
	    first = false;
	}
	else
	{
	    fprintf(stderr, ", %d", arr[i]);
	}
    }

    fprintf(stderr, "]\n");
 
}

void print_last_block(std::array<bin_int, ZOBRIST_LAST_BLOCKSIZE> arr)
{
    bool first = true;
    for (int i = 0; i < ZOBRIST_LAST_BLOCKSIZE; i++)
    {
	if(first)
	{
	    fprintf(stderr, "[%d", arr[i]);
	    first = false;
	}
	else
	{
	    fprintf(stderr, ", %d", arr[i]);
	}
    }

    fprintf(stderr, "]\n");
}
       
std::array<bin_int, ZOBRIST_LOAD_BLOCKSIZE> decode_position(int pos)
{
    std::array<bin_int, ZOBRIST_LOAD_BLOCKSIZE> ret = {0};
    for (int i = ZOBRIST_LOAD_BLOCKSIZE-1; i >= 0 ; i--)
    {
	bin_int current_value = (bin_int) (pos % (R+1));
	ret[i] = current_value;
	pos /= (R+1);
    }
    return ret;
}

std::array<bin_int, ZOBRIST_LAST_BLOCKSIZE> decode_last_position(int pos)
{
    std::array<bin_int, ZOBRIST_LAST_BLOCKSIZE> ret = {0};
    for (int i = ZOBRIST_LAST_BLOCKSIZE-1; i >= 0 ; i--)
    {
	bin_int current_value = (bin_int) (pos % (R+1));
	ret[i] = current_value;
	pos /= (R+1);
    }
    return ret;
}

uint64_t loadhash_from_position(uint64_t *zl, int blocknum, int pos)
{

    uint64_t ret = 0;
    std::array<bin_int, ZOBRIST_LOAD_BLOCKSIZE> pos_arr = decode_position(pos);
    for (int i = 0; i < ZOBRIST_LOAD_BLOCKSIZE; i++)
    {
	int bin_index = blocknum*ZOBRIST_LOAD_BLOCKSIZE + i;
	ret ^= zl[bin_index*(R+1) + pos_arr[i]];
    }

    return ret;
}

uint64_t loadhash_last_from_position(uint64_t *zl, int pos)
{
    uint64_t ret = 0;
    std::array<bin_int, ZOBRIST_LAST_BLOCKSIZE> pos_arr = decode_last_position(pos);
    for (int i = 0; i < ZOBRIST_LAST_BLOCKSIZE; i++)
    {
	int bin_index = (ZOBRIST_LOAD_BLOCKS-1)*ZOBRIST_LOAD_BLOCKSIZE + i;
	ret ^= zl[bin_index*(R+1) + pos_arr[i]];
    }

    return ret;
}


void zobrist_init()
{
    // Zl generates load hashes the classical way -- one for each load position.
    
    Zl = new uint64_t[(BINS+1)*(R+1)];

    // Zlbig is the new way of representing load hashes.
    // Basically, we store each sequence of 5 bins as a block.
    Zlbig = new uint64_t*[ZOBRIST_LOAD_BLOCKS];
    for (int bl = 0; bl <= ZOBRIST_LOAD_BLOCKS - 2; bl++)
    {
	Zlbig[bl] = new uint64_t[power<int>((R+1), ZOBRIST_LOAD_BLOCKSIZE)];
    }

    // Zlbig last position.
    Zlbig[ZOBRIST_LOAD_BLOCKS-1] = new uint64_t[power<int>((R+1),ZOBRIST_LAST_BLOCKSIZE)];
    
    for (int i=0; i<=BINS; i++)
    {
	for (int j=0; j<=R; j++)
	{
	    Zl[i*(R+1)+j] = rand_64bit();
	}
    }

    for (int bl = 0; bl <= ZOBRIST_LOAD_BLOCKS - 2; bl++)
    {
	for (int pos = 0; pos < power<int>((R+1),ZOBRIST_LOAD_BLOCKSIZE); pos++)
	{
	    Zlbig[bl][pos] = loadhash_from_position(Zl, bl, pos);
	}
    }

    // Zlbig last position

    for (int pos = 0; pos < power<int>((R+1),ZOBRIST_LAST_BLOCKSIZE); pos++)
    {
	Zlbig[ZOBRIST_LOAD_BLOCKS-1][pos] = loadhash_last_from_position(Zl, pos);
    }
}

inline uint64_t independent_hash(std::array<int, BINS> loads)
{
    uint64_t hash = 0;
    for(int i = 0; i < BINS; i++)
    {
	hash ^= Zl[(i)*(R+1) + loads[i]];
    }
    return hash;
}

inline uint64_t lookup_hash(std::array<int, BINS> loads)
{
    uint64_t hash = 0;
    int pos = 0;

    // Normal blocks.
    for (int bl = 0; bl <= ZOBRIST_LOAD_BLOCKS - 2; bl++)
    {
	pos = 0;
	for (int el = 0; el < ZOBRIST_LOAD_BLOCKSIZE; el++)
	{
	    pos *= (R+1);
	    if (DEBUG)
	    {
		fprintf(stderr, "Multiplying by %d.\n", R+1);
	    }
	    
	    pos += loads[bl*ZOBRIST_LOAD_BLOCKSIZE + el];
	    
	    if (DEBUG)
	    {
		fprintf(stderr, "Decoded position:\n");
		print_standard_block(decode_position(pos));
		fprintf(stderr, "Adding in the load at position %d, which is %d.\n",
			bl*ZOBRIST_LOAD_BLOCKSIZE+el, loads[bl*ZOBRIST_LOAD_BLOCKSIZE+el]);
	    }
	}

	// Rolling check. Disable when measuring performance.
	if (DEBUG)
	{
	    uint64_t hashcheck = 0;
	    for (int el = 0; el < ZOBRIST_LOAD_BLOCKSIZE; el++)
	    {
		fprintf(stderr, "Multiplying by %d *(R+1) and adding load %d.\n ",
			(bl*ZOBRIST_LOAD_BLOCKSIZE+el), loads[bl*ZOBRIST_LOAD_BLOCKSIZE + el]);
		hashcheck ^= Zl[(bl*ZOBRIST_LOAD_BLOCKSIZE+el) * (R+1) + loads[bl*ZOBRIST_LOAD_BLOCKSIZE + el]];
	    }

	    fprintf(stderr, "Hash at Zlbig[bl][pos] (Zlbig[%d][%d]) = %" PRIu64 " should match the rolling hash %" PRIu64 ".\n", bl, pos, Zlbig[bl][pos], hashcheck);
	}
	
	hash ^= Zlbig[bl][pos];
    }
    // Last block.
    pos = 0;
    for (int el = 0; el < ZOBRIST_LAST_BLOCKSIZE; el++)
    {
	pos *= (R+1);
	if (DEBUG)
	{
	    fprintf(stderr, "FINAL: Multiplying by %d.\n", R+1);
	}
	pos += loads[(ZOBRIST_LOAD_BLOCKS-1)*ZOBRIST_LOAD_BLOCKSIZE + el];

	if (DEBUG)
	{
	    fprintf(stderr, "FINAL: Adding in the load at position %d, which is %d.\n", (ZOBRIST_LOAD_BLOCKS-1)*ZOBRIST_LOAD_BLOCKSIZE + el, loads[(ZOBRIST_LOAD_BLOCKS-1)*ZOBRIST_LOAD_BLOCKSIZE + el]);
	}
    }

    if (DEBUG)
    {
	fprintf(stderr, "Position in the last block (%d) is %d.\n", (ZOBRIST_LOAD_BLOCKS-1), pos);
    }
    hash ^= Zlbig[ZOBRIST_LOAD_BLOCKS-1][pos];

    return hash;
}

void correctness_checks()
{
    
    // A quick zobrist test.
    int load0 = rand_load();
    int load1 = rand_load() % (load0+1);
    int load2 = rand_load() % (load1+1);
    int load3 = rand_load() % (load2+1);
    int load4 = rand_load() % (load3+1);
    int load5 = rand_load() % (load4+1);
    int load6 = rand_load() % (load5+1);
    int load7 = rand_load() % (load6+1);
    int load8 = rand_load() % (load7+1);
    int load9 = rand_load() % (load8+1);

    std::array<int, BINS> ten_arr = {load0, load1, load2, load3, load4,
	load5, load6, load7, load8, load9};


    bool first = true;
    for (int i = 0; i < BINS; i++)
    {
	if(first)
	{
	    fprintf(stderr, "[%d", ten_arr[i]);
	    first = false;
	}
	else
	{
	    fprintf(stderr, ", %d", ten_arr[i]);
	}
    }

    fprintf(stderr, "]\n");
    
    uint64_t hash1 = independent_hash(ten_arr);
    uint64_t hash2 = lookup_hash(ten_arr);

    // fprintf(stderr, "If the first block is loaded to [%d, %d, %d, %d, %d] ",
    // 	load1, load2, load3, load4, load5);
    // fprintf(stderr, "the hashes match.\n");
    
    if (hash2 != hash1)
    {
	fprintf(stderr, "The hashes do not match for the first block [%d, %d, %d, %d, %d].\n ",
		load1, load2, load3, load4, load5);
	assert(hash2 == hash1);
    }
}

uint64_t random_independent_hash()
{
    // A quick zobrist test.
    int load0 = rand_load();
    int load1 = rand_load() % (load0+1);
    int load2 = rand_load() % (load1+1);
    int load3 = rand_load() % (load2+1);
    int load4 = rand_load() % (load3+1);
    int load5 = rand_load() % (load4+1);
    int load6 = rand_load() % (load5+1);
    int load7 = rand_load() % (load6+1);
    int load8 = rand_load() % (load7+1);
    int load9 = rand_load() % (load8+1);

    std::array<int, BINS> ten_arr = {load0, load1, load2, load3, load4,
	load5, load6, load7, load8, load9};


    return independent_hash(ten_arr);

}

uint64_t random_lookup_hash()
{
        // A quick zobrist test.
    int load0 = rand_load();
    int load1 = rand_load() % (load0+1);
    int load2 = rand_load() % (load1+1);
    int load3 = rand_load() % (load2+1);
    int load4 = rand_load() % (load3+1);
    int load5 = rand_load() % (load4+1);
    int load6 = rand_load() % (load5+1);
    int load7 = rand_load() % (load6+1);
    int load8 = rand_load() % (load7+1);
    int load9 = rand_load() % (load8+1);

    std::array<int, BINS> ten_arr = {load0, load1, load2, load3, load4,
	load5, load6, load7, load8, load9};


    return lookup_hash(ten_arr);
}

int main(void)
{
    // Init zobrist with fixed random numbers (as would be used in the actual search code).
    zobrist_init();
    // Regenerate the mersenne twister so that correctness checks have different numbers each run.
    unsigned time_seed = std::chrono::system_clock::now().time_since_epoch().count();
    gen = std::mt19937_64(time_seed);
    
    correctness_checks();
    correctness_checks();


    std::vector<uint64_t> results;
    results.reserve(TESTS);

    for (int i =0; i < TESTS; i++)
    {
	results.push_back(random_lookup_hash());
    }
}
