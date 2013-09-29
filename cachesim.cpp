#include "cachesim.hpp"
#include "cache.c"
Cache l1, l2;
int access;

void tally(cache_stats_t* p_stats, CacheStatus status, int level, char rw)
{
    if(level == 1){
      p_stats->L1_accesses++;
    }

    if(status == HIT || status == PREFETCH_HIT)
    {
      if(status == PREFETCH_HIT)
              p_stats->successful_prefetches++;

    }else{
      if(rw == READ){
        if(level == 1){
          p_stats->L1_read_misses++;
        }else{
          p_stats->L2_read_misses++;
        }
      }else{
        if(level == 1){
          p_stats->L1_write_misses++;
        }else{
          p_stats->L2_write_misses++;
        }
      }
      if(status == WRITE_BACK && level == 2)
        p_stats->write_backs++;
    }
}

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
  access = 0;
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
  //printf("%i", access);
  if(rw == READ)
  {
    p_stats->reads++;
    status = Cache_read(&l1, address);
    tally(p_stats, status, 1, rw);
    if(status == MISS)
    {
          status = Cache_read(&l2, address);
          tally(p_stats, status, 2, rw);
    }
    else if(status == WRITE_BACK)
    {
          status = Cache_write(&l2, l1.write_back);
          tally(p_stats, status, 2, rw);
    }
  }else{
    p_stats->writes++;
    status = Cache_write(&l1, address);
    tally(p_stats, status, 1, rw);
    if(status == MISS)
    {
      status = Cache_write(&l2, address);
      tally(p_stats, status, 2, rw);
    }
    else if(status == WRITE_BACK)
    {
      status = Cache_write(&l2, l1.write_back);
      tally(p_stats, status, 2, rw);
      status = Cache_write(&l2, address);
      tally(p_stats, status, 2, rw);
    }
  }
  //printf("\n");
  access++;
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
