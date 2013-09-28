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
  int bytes_p_b = pow(2, pCache->b);
  int num_set = 16;
  //printf("L%d cache created: tag_size=%llu, index_size=%i, offset_size=%llu, bytes_per_block=%i, blocks_per_set=%llu, num_set=%i\n", level, 64 - pCache->b - Cache_index_length(pCache), Cache_index_length(pCache), pCache->b, bytes_p_b, pCache->ways, num_set);
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
  uint64_t victim_lookup = index;
  //printf("|w|%llx", address);

  for(int way = 0; way < pCache->ways; way++)
  {
    uint64_t lookup = Cache_lookup_calc(pCache, way, index);
    if(!pCache->valid[lookup] || pCache->last_access[lookup] < pCache->last_access[victim_lookup])
    {
      victim_lookup = lookup;
    }

    if(pCache->valid[lookup] && pCache->tagstore[lookup] == tag)
    {
      //printf("|L%dWriteHit", pCache->level);
      pCache->dirty[lookup] = true;
      time(&pCache->last_access[lookup]); // Freshen the LRU
      if(pCache->prefetched[lookup])
      {
        pCache->prefetched[lookup] = false;
        return PREFETCH_HIT;
      }
      return HIT;
    }
  }
  //printf("|L%dWriteMiss", pCache->level);

  CacheStatus ret_val;
  if(pCache->dirty[victim_lookup])
  {
    Cache_set_write_back(pCache, victim_lookup);
    ret_val = WRITE_BACK;
  }else{
    ret_val = MISS;
  }
  pCache->tagstore[victim_lookup] = tag;
  pCache->valid[victim_lookup] = true;
  time(&pCache->last_access[victim_lookup]);

  return ret_val;
}

CacheStatus Cache_prefetch(Cache *pCache, uint64_t address)
{
  uint64_t index = Cache_index_calc(pCache, address);
  uint64_t tag = Cache_tag_calc(pCache, address);
  uint64_t victim_lookup = index;

  for(int way = 0; way < pCache->ways; way++)
  {
    uint64_t lookup = Cache_lookup_calc(pCache, way, index);
    //Bookkeep who we should evict if needed
    if(!pCache->valid[lookup] || pCache->last_access[lookup] < pCache->last_access[victim_lookup])
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

CacheStatus Cache_read(Cache *pCache, uint64_t address)
{
  uint64_t index = Cache_index_calc(pCache, address);
  uint64_t tag = Cache_tag_calc(pCache, address);
  uint64_t victim_lookup = index;
  //printf("|r|%llx", address);

  for(int way = 0; way < pCache->ways; way++)
  {
    uint64_t lookup = Cache_lookup_calc(pCache, way, index);
    //Bookkeep who we should evict if needed
    if(!pCache->valid[lookup] || pCache->last_access[lookup] < pCache->last_access[victim_lookup])
    {
      victim_lookup = lookup;
    }

    if(pCache->valid[lookup] && pCache->tagstore[lookup] == tag)
    {
      //printf("|L%dReadHit", pCache->level);
      time(&pCache->last_access[lookup]); // Freshen the LRU
      if(pCache->prefetched[lookup])
      {
        pCache->prefetched[lookup] = false;
        return PREFETCH_HIT;
      }
      return HIT;
    }
  }
  //printf("|L%dReadMiss", pCache->level);

  CacheStatus ret_val;

  if(pCache->valid[victim_lookup] && pCache->dirty[victim_lookup])
  {
    pCache->dirty[victim_lookup] = false;
    Cache_set_write_back(pCache, victim_lookup);
    ret_val = WRITE_BACK;
  }else{
    ret_val = MISS;
  }
  pCache->tagstore[victim_lookup] = tag;
  pCache->valid[victim_lookup] = true;
  time(&pCache->last_access[victim_lookup]);
  return ret_val;
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
  int chunk = pCache->lines / pCache->ways;
  return (way * chunk) + index;
}

int Cache_index_length(Cache *pCache)
{
  return pCache->c - pCache->s - pCache->b;
}

uint64_t Cache_index_calc(Cache *pCache, uint64_t address)
{
  address = address >> pCache->b; // throw away offset
  address &= createMask(0, Cache_index_length(pCache));
  return address;
}

uint64_t Cache_lines(Cache *pCache)
{
  return pow(2, pCache->c)/pow(2, pCache->b);
}

unsigned createMask(unsigned a, unsigned b)
{
  unsigned r = 0;
  for (unsigned i=a; i<=b; i++)
    r |= 1 << i;
  return r;
}
