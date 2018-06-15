#include <cstdio>
#include <cassert>
#include <atomic>

class conf_el
{
public:
    uint64_t _data;
};

class dpht_el
{
public:
    uint64_t _hash;
    int16_t _feasible;
    int16_t _empty_bins;
    int16_t _permanence;
    int16_t _unused; //align to 128 bit.
};

std::atomic<conf_el> ht;
std::atomic<dpht_el> dpht;
std::atomic<__int128> bigint;

int main(void)
{
    bigint.store(5);
    conf_el y{0};
    ht.store(y);
    dpht_el x{0};
    dpht.store(x);
    return 0;
}
