#include "cache.h"

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
  pCache->last_access = (time_t *)calloc(pCache->lines, sizeof(time_t));

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

  ret_val = Cache_find(pCache, tag, index, true);
  if(ret_val != MISS)
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
  time(&pCache->last_access[victim_lookup]);

  return ret_val;
}

CacheStatus Cache_prefetch(Cache *pCache, uint64_t address)
{
  unsigned way;
  uint64_t index = Cache_index_calc(pCache, address);
  uint64_t tag = Cache_tag_calc(pCache, address);
  uint64_t victim_lookup = index;

  for(way = 0; way < pCache->ways; way++)
  {
    uint64_t lookup = Cache_lookup_calc(pCache, way, index);
    //Bookkeep who we should evict if needed
    if(pCache->last_access[lookup] < pCache->last_access[victim_lookup])
    {
      victim_lookup = lookup;
    }

    if(pCache->valid[lookup] && pCache->tagstore[lookup] == tag)
    {
      return HIT;
    }
  }

  CacheStatus ret_val;
  if(pCache->valid[victim_lookup] && pCache->dirty[victim_lookup])
  {
     Cache_set_write_back(pCache, victim_lookup);
     ret_val = WRITE_BACK;
  }else{
     ret_val = MISS;
  }
  pCache->last_access[victim_lookup] = 0;
  pCache->prefetched[victim_lookup] = true;
  pCache->tagstore[victim_lookup] = tag;
  pCache->dirty[victim_lookup] = false;
  pCache->valid[victim_lookup] = true;
  return ret_val;
}

uint64_t Cache_execute_prefetch(Cache *pCache)
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
    }
  }
  pCache->pending_stride = d;
  return count;
}

CacheStatus Cache_read(Cache *pCache, uint64_t address)
{
  uint64_t index = Cache_index_calc(pCache, address);
  uint64_t tag = Cache_tag_calc(pCache, address);
  CacheStatus ret_val;

  ret_val = Cache_find(pCache, tag, index, false);
  if(ret_val != MISS)
          return ret_val;

  uint64_t victim_lookup = Cache_victim_lookup(pCache, tag, index);
  if(pCache->valid[victim_lookup] && pCache->dirty[victim_lookup])
  {
    pCache->dirty[victim_lookup] = false;
    Cache_set_write_back(pCache, victim_lookup);
    ret_val = WRITE_BACK;
  }

  pCache->tagstore[victim_lookup] = tag;
  pCache->prefetched[victim_lookup] = false;
  pCache->valid[victim_lookup] = true;
  time(&pCache->last_access[victim_lookup]);
  return ret_val;
}

CacheStatus Cache_find(Cache* pCache, uint64_t tag, uint64_t index, bool dirty)
{
  unsigned way;
  for(way = 0; way < pCache->ways; way++)
  {
    uint64_t lookup = Cache_lookup_calc(pCache, way, index);
    if(pCache->valid[lookup] && pCache->tagstore[lookup] == tag)
    {
      pCache->dirty[lookup] = dirty;
      time(&pCache->last_access[lookup]); // Freshen the LRU
      if(pCache->prefetched[lookup])
      {
        pCache->prefetched[lookup] = false;
        return PREFETCH_HIT;
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
  time_t min_time = pCache->last_access[min_lookup];

  for(way = 1; way < pCache->ways; way++)
  {
    uint64_t lookup = Cache_lookup_calc(pCache, way, index);
    if(!pCache->valid[lookup] || pCache->last_access[lookup] < min_time)
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

uint64_t Cache_lookup_calc(Cache *pCache, int way, uint64_t index)
{
  int chunkSize = pCache->lines / pCache->ways;
  uint64_t lookup = (way * chunkSize) + index;
  //printf("Way: %i chunkSize: %i index: %llu lookup: %llu\n", way, chunkSize, index, lookup);
  if(lookup >= pCache->lines)
                 exit(34);
  return lookup;
}

int Cache_index_length(Cache *pCache)
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
