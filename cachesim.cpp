#include "cachesim.hpp"


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
        clock_t* last_access;
        clock_t clock;
        int level;
        uint64_t pending_stride, last_miss_addr;
        uint64_t write_back, prefetch_addr;
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
clock_t Cache_min_lru(Cache *pCache, uint64_t index);
uint64_t Cache_execute_prefetch(Cache *pCache, cache_stats_t*);
CacheStatus Cache_read(Cache*, uint64_t address);
CacheStatus Cache_write(Cache*, uint64_t address);
CacheStatus Cache_prefetch(Cache*, uint64_t address);
CacheStatus Cache_find(Cache*, uint64_t tag, uint64_t index, bool dirty, bool);
void Cache_set_write_back(Cache*, uint64_t line);
uint64_t Cache_victim_lookup(Cache* pCache, uint64_t tag, uint64_t index);
uint64_t Cache_tag_calc(Cache *pCache, uint64_t address);
uint64_t Cache_lookup_calc(Cache *pCache, unsigned way, uint64_t index);
unsigned Cache_index_length(Cache*);
uint64_t Cache_index_calc(Cache *pCache, uint64_t address);
uint64_t Cache_lines(Cache*);
unsigned createMask(unsigned, unsigned);

void Cache_construct(Cache *pCache, uint64_t c, uint64_t b, uint64_t s, int level)
{
  pCache->c = c;
  pCache->b = b;
  pCache->s = s;
  pCache->level = level;
  pCache->ways = pow(2, pCache->s);
  pCache->lines = Cache_lines(pCache);
  pCache->tagstore = (uint64_t *)calloc(pCache->lines, sizeof(uint64_t));
  pCache->dirty = (bool *)calloc(pCache->lines, sizeof(bool));
  pCache->valid = (bool *)calloc(pCache->lines, sizeof(bool));
  pCache->prefetched = (bool *)calloc(pCache->lines, sizeof(bool));
  pCache->last_access = (clock_t *)calloc(pCache->lines, sizeof(clock_t));
  pCache->clock = 0;

  if(!pCache->tagstore || !pCache->dirty || !pCache->prefetched || !pCache->last_access || !pCache->valid)
  {
    printf("could not allocate");
    exit(0);
  }
}

void Cache_destroy(Cache *pCache)
{
  free(pCache->tagstore);
  free(pCache->dirty);
  free(pCache->valid);
  free(pCache->prefetched);
  free(pCache->last_access);
}

CacheStatus Cache_write(Cache *pCache, uint64_t address)
{
  uint64_t index = Cache_index_calc(pCache, address);
  uint64_t tag = Cache_tag_calc(pCache, address);
  CacheStatus ret_val;

  ret_val = Cache_find(pCache, tag, index, true, true);
  if(ret_val == HIT || ret_val == PREFETCH_HIT)
          return ret_val;

  uint64_t victim_lookup = Cache_victim_lookup(pCache, tag, index);
  if(pCache->dirty[victim_lookup])
  {
    Cache_set_write_back(pCache, victim_lookup);
    ret_val = WRITE_BACK;
  }
  pCache->tagstore[victim_lookup] = tag;
  pCache->valid[victim_lookup] = true;
  pCache->prefetched[victim_lookup] = false;
  pCache->last_access[victim_lookup] = pCache->clock;

  return ret_val;
}

CacheStatus Cache_prefetch(Cache *pCache, uint64_t address)
{
  uint64_t index = Cache_index_calc(pCache, address);
  uint64_t tag = Cache_tag_calc(pCache, address);
  CacheStatus ret_val;

  ret_val = Cache_find(pCache, tag, index, false, false);
  if(ret_val == HIT)
          return ret_val;

  uint64_t victim_lookup = Cache_victim_lookup(pCache, tag, index);

  if(pCache->dirty[victim_lookup])
  {
     ret_val = WRITE_BACK;
  }else{
     ret_val = MISS;
  }
  pCache->last_access[victim_lookup] = Cache_min_lru(pCache, index) - 1;
  pCache->prefetched[victim_lookup] = true;
  pCache->tagstore[victim_lookup] = tag;
  pCache->dirty[victim_lookup] = false;
  pCache->valid[victim_lookup] = true;
  return ret_val;
}

clock_t Cache_min_lru(Cache *pCache, uint64_t index)
{
  clock_t min = pCache->clock + 1;
  uint64_t way;
  uint64_t chunkSize = pCache->lines / pCache->ways;
  for(way = 0; way < pCache->ways; way++)
  {
    clock_t tmp = pCache->last_access[(way*chunkSize) + index];
    if(tmp < min)
            min = tmp;
  }
  if(min <= 0)
          return 1;
  return min;
}
uint64_t Cache_execute_prefetch(Cache *pCache, cache_stats_t* p_stats)
{
  unsigned i;
  uint64_t x_block = (pCache->prefetch_addr >> pCache->b) << pCache->b; //zero offset
  uint64_t d = x_block - pCache->last_miss_addr;
  pCache->last_miss_addr = x_block;
  uint64_t count = 0;
  if(d == pCache->pending_stride)
  {
    for(i = 1; i <= pCache->k; i++)
    {
       CacheStatus status = Cache_prefetch(pCache, (pCache->prefetch_addr + (i * pCache->pending_stride)));
       if(status != HIT)
        count++;
       if(status == WRITE_BACK)
        p_stats->write_backs++;
    }
  }
  pCache->pending_stride = d;
  return count;
}

CacheStatus Cache_read(Cache *pCache, uint64_t address)
{
  //printf("0x%llx @ L%i\n", address, pCache->level);
  uint64_t index = Cache_index_calc(pCache, address);
  uint64_t tag = Cache_tag_calc(pCache, address);
  //printf("0x%llu 0x%llu\n", tag, index);
  CacheStatus ret_val;

  ret_val = Cache_find(pCache, tag, index, false, true);
  if(ret_val == HIT || ret_val == PREFETCH_HIT)
          return ret_val;

  uint64_t victim_lookup = Cache_victim_lookup(pCache, tag, index);
  //printf("\tVictim Line: %llu\n", victim_lookup);
  if(pCache->valid[victim_lookup] && pCache->dirty[victim_lookup])
  {
    //printf("\tWRITE_BACK\n");
    pCache->dirty[victim_lookup] = false;
    Cache_set_write_back(pCache, victim_lookup);
    ret_val = WRITE_BACK;
  }

  pCache->tagstore[victim_lookup] = tag;
  pCache->prefetched[victim_lookup] = false;
  pCache->valid[victim_lookup] = true;
  pCache->last_access[victim_lookup] = pCache->clock;
  return ret_val;
}

CacheStatus Cache_find(Cache* pCache, uint64_t tag, uint64_t index, bool dirty, bool bookkeep)
{
  unsigned way;
  for(way = 0; way < pCache->ways; way++)
  {
    uint64_t lookup = Cache_lookup_calc(pCache, way, index);
    //if(pCache->level == 2)
    //        printf("\tlookup: %llu tag: %llu tagstore: %llu\n", lookup, tag, pCache->tagstore[lookup]);
    if(pCache->valid[lookup] && pCache->tagstore[lookup] == tag)
    {
      if(bookkeep)
      {
              pCache->dirty[lookup] = dirty;
              pCache->last_access[lookup] = pCache->clock; // Freshen the LRU
      if(pCache->prefetched[lookup])
      {
        pCache->prefetched[lookup] = false;
        return PREFETCH_HIT;
      }
      }
      return HIT;
    }
  }
  return MISS;
}

uint64_t Cache_victim_lookup(Cache* pCache, uint64_t tag, uint64_t index)
{
  unsigned way;
  uint64_t min_lookup = Cache_lookup_calc(pCache, 0, index);
  clock_t min_time = pCache->last_access[min_lookup];
  if(min_time == 0)
          return min_lookup;
  for(way = 1; way < pCache->ways; way++)
  {
    uint64_t lookup = Cache_lookup_calc(pCache, way, index);
    //printf("\tPossible Victim\n");
    //printf("\t\tline: %llu way: %u time: %ld\n", lookup, way, pCache->last_access[lookup]);
    if(pCache->last_access[lookup] < min_time)
    {
      min_lookup = lookup;
      min_time = pCache->last_access[lookup];
    }
  }
  return min_lookup;
}

void Cache_set_write_back(Cache *pCache, uint64_t line)
{
  pCache->write_back = pCache->tagstore[line] << (pCache->b + Cache_index_length(pCache));
  while(line >= pCache->lines)
    line -= pCache->lines;
  pCache->write_back = pCache->write_back || line;
  pCache->write_back = pCache->write_back << pCache->b;
}

uint64_t Cache_tag_calc(Cache *pCache, uint64_t address)
{
  return address >> (pCache->b + Cache_index_length(pCache));
}

uint64_t Cache_lookup_calc(Cache *pCache, unsigned way, uint64_t index)
{
  unsigned chunkSize = pCache->lines / pCache->ways;
  uint64_t lookup = (way * chunkSize) + index;
  if(lookup >= pCache->lines)
                 exit(34);
  return lookup;
}

unsigned Cache_index_length(Cache *pCache)
{
  return pCache->c - pCache->s - pCache->b;
}

uint64_t Cache_index_calc(Cache *pCache, uint64_t address)
{
  address = address >> pCache->b; // throw away offset
  address &= createMask(0, Cache_index_length(pCache) - 1);
  return address;
}

uint64_t Cache_lines(Cache *pCache)
{
  return pow(2, pCache->c)/pow(2, pCache->b);
}

unsigned createMask(unsigned a, unsigned b)
{
  unsigned i;
  unsigned r = 0;
  for (i=a; i<=b; i++)
    r |= 1 << i;
  return r;
}
Cache l1, l2;

/**
 * Subroutine for initializing the cache. You many add and initialize any global or heap
 * variables as needed.
 *
 * @c1 Total size of L1 in bytes is 2^C1
 * @b1 Size of each block in L1 in bytes is 2^B1
 * @s1 Number of blocks per set in L1 is 2^S1
 * @c2 Total size of L2 in bytes is 2^C2
 * @b2 Size of each block in L2 in bytes is 2^B2
 * @s2 Number of blocks per set in L2 is 2^S2
 * @k Prefetch K subsequent blocks
 */
void setup_cache(uint64_t c1, uint64_t b1, uint64_t s1, uint64_t c2, uint64_t b2, uint64_t s2, uint32_t k) {
  Cache_construct(&l1, c1, b1, s1, 1);
  Cache_construct(&l2, c2, b2, s2, 2);
  l2.k = k;
}

/**
 * Subroutine that simulates the cache one trace event at a time.
 *
 * @rw The type of event. Either READ or WRITE
 * @address  The target memory address
 * @p_stats Pointer to the statistics structure
 */
void cache_access(char rw, uint64_t address, cache_stats_t* p_stats) {
  CacheStatus status;
  p_stats->L1_accesses++;
  if(rw == READ)
  {
    p_stats->reads++;
    status = Cache_read(&l1, address);
  }else{
    p_stats->writes++;
    status = Cache_write(&l1, address);
  }

  if(status == HIT || status == PREFETCH_HIT)
    return;

  //L2 is involved
  if(status == WRITE_BACK)
  {
    status = Cache_write(&l2, l1.write_back);
    if(status == MISS || status == WRITE_BACK)
            p_stats->L2_write_misses++;
    if(status == WRITE_BACK)
            p_stats->write_backs++;
  }

  if(rw == READ)
  {
     p_stats->L1_read_misses++;
     status = Cache_read(&l2, address);
  }else{
     p_stats->L1_write_misses++;
     status = Cache_write(&l2, address);
  }

  if(status == PREFETCH_HIT)
    p_stats->successful_prefetches++;

  if(status == WRITE_BACK)
     p_stats->write_backs++;

  if(status == MISS || status == WRITE_BACK)
  {
     if(rw == READ)
        p_stats->L2_read_misses++;
     if(rw == WRITE)
        p_stats->L2_write_misses++;
     l2.prefetch_addr = address;
     p_stats->prefetched_blocks += Cache_execute_prefetch(&l2, p_stats);
  }
  l1.clock++;
  l2.clock++;
  //printf("\n");
}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(cache_stats_t *p_stats) {
  double MP1, MR1, MR2;
  double HT1 = 2 + 0.2*l1.s;
  double HT2 = 4 + 0.4*l2.s;
  double MP2 = 500.0;

  MR1 = (double)(p_stats->L1_read_misses + p_stats->L1_write_misses)/(double)p_stats->L1_accesses;
  MR2 = (double)p_stats->L2_read_misses/(double)(p_stats->L1_read_misses + p_stats->L2_read_misses);
  MP1 = HT2 + (MR2 * MP2);
  p_stats->avg_access_time = HT1 + (MR1 * MP1);
  Cache_destroy(&l1);
  Cache_destroy(&l2);
}
