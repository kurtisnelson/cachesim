#ifndef CACHE_H
#define CACHE_H
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

typedef struct
{
        uint64_t* tagstore;
        bool* dirty;
        bool* prefetched;
        bool* valid;
        time_t* last_access;
        int level;
        uint64_t write_back;
        uint64_t c, b, s, k;
        uint64_t lines, ways;
} Cache;

typedef enum
{
        WRITE_BACK,
        PREFETCH_HIT,
        HIT,
        MISS
} CacheStatus;

void Cache_construct(Cache*, uint64_t c, uint64_t b, uint64_t s, int level);
void Cache_destroy(Cache*);
CacheStatus Cache_read(Cache*, uint64_t address);
CacheStatus Cache_write(Cache*, uint64_t address);
CacheStatus Cache_prefetch(Cache*, uint64_t address);
CacheStatus Cache_find(Cache*, uint64_t tag, uint64_t index, bool dirty);
void Cache_set_write_back(Cache*, uint64_t line);
uint64_t Cache_victim_lookup(Cache* pCache, uint64_t tag, uint64_t index);
uint64_t Cache_tag_calc(Cache *pCache, uint64_t address);
uint64_t Cache_lookup_calc(Cache *pCache, int way, uint64_t index);
int Cache_index_length(Cache*);
uint64_t Cache_index_calc(Cache *pCache, uint64_t address);
uint64_t Cache_lines(Cache*);
unsigned createMask(unsigned, unsigned);
#endif
