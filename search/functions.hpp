#ifndef _FUNCTIONS_HPP
#define _FUNCTIONS_HPP

// Small helper functions that do not need to be adjusted often.

// A Windows/Linux inclusion:

#ifdef _WIN32
#include <winsock.h>
#else
#include <unistd.h>
#endif

#include <string>

std::string gethost()
{

    char buf[0x100];
    if( gethostname(buf, sizeof(buf)) == 0 )
    {
	return std::string(buf);
    }

    return std::string();
}

// A fprintf(stderr) version of the #error macro.
void ERROR(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);

    abort();
}

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

// -- Binary and hashing. --
void binary_print(FILE* stream, uint64_t num)
{
    uint64_t bit = 0;
    for (int i = 63; i >= 0; i--)
    {
	bit = num >> i;
	if (bit % 2 == 0)
	{
	    fprintf(stream, "0");
	} else
	{
	    fprintf(stream, "1");
	}
    }
}


// Zeroes last bit of a number -- useful to check hashes.
inline uint64_t zero_last_bit(uint64_t n)
{
    return ((n >> 1) << 1);
}

uint64_t zero_last_two_bits(const uint64_t& n)
{
    return ((n >> 2) << 2);
}

bin_int get_last_two_bits(const uint64_t& n)
{
    return ((n & 3));
}

inline bool get_last_bit(uint64_t n)
{
    return ((n & 1) == 1);
}

uint64_t quicklog(uint64_t x)
{
    uint64_t ret = 0;
    while(x >>= 1)
    {
	ret++;
    }
    return ret;
}

// Powering 2^X.
uint64_t two_to(uint64_t x)
{
    return 1LLU << x;
}

// A math routine computing the largest power of two less than x.
uint64_t power_of_two_below(uint64_t x)
{
    return two_to(quicklog(x));
}

void printBits32(unsigned int num)
{
   for(unsigned int bit=0;bit<(sizeof(unsigned int) * 8); bit++)
   {
      fprintf(stderr, "%i", num & 0x01);
      num = num >> 1;
   }
   fprintf(stderr, "\n");
}

void printBits64(llu num)
{
   for(unsigned int bit=0;bit<(sizeof(llu) * 8); bit++)
   {
      fprintf(stderr, "%" PRIu64, num & 0x01);
      num = num >> 1;
   }
   fprintf(stderr, "\n");
}

// template logpart

template<unsigned int LOG> inline uint64_t logpart(uint64_t x)
{
    return x >> (64 - LOG); 
}

const auto loadlogpart = logpart<LOADLOG>;

// with some memory allocated dynamically, we also need a dynamic logpart

inline uint64_t logpart(uint64_t x, int log)
{
    return x >> (64 - log);
}


// printing an array of constant size in a reasonable way

template <int NUM> void print_int_array(FILE *stream,
					const std::array<int, NUM>& arr,
					bool trailing_newline = false)
{
    fprintf(stream, "[");
    for (int i =0; i < NUM; i++)
    {
	fprintf(stream, "%d", arr[i]);
	if (i < NUM-1)
	{
	    fprintf(stream, " ");
	}
    }
    fprintf(stream, "]");
    if (trailing_newline)
    {
	fprintf(stream, "\n");
    }
}

template <int NUM> void print_int_array(const std::array<int, NUM>& arr,
					bool trailing_newline = false)
{
    print_int_array<NUM>(stderr, arr, trailing_newline); 
}
#endif
