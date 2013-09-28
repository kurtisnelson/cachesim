#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "cachesim.hpp"

void print_help_and_exit(void) {
    printf("cachesim [OPTIONS] < traces/file.trace\n");
    printf("  -c C1\t\tTotal size of L1 in bytes is 2^C1\n");
    printf("  -b B1\t\tSize of each block in L1 in bytes is 2^B1\n");
    printf("  -s S1\t\tNumber of blocks per set in L1 is 2^S1\n");
    printf("  -C C2\t\tTotal size of L2 in bytes is 2^C2\n");
    printf("  -B B2\t\tSize of each block in L2 in bytes is 2^B2\n");
    printf("  -S S2\t\tNumber of blocks per set in L2 is 2^S2\n");
    printf("  -k K\t\tNumber of prefetch blocks\n");
    printf("  -h\t\tThis helpful output\n");
    exit(0);
}

void print_statistics(cache_stats_t* p_stats);

int main(int argc, char* argv[]) {
    int opt;
    uint64_t c1 = DEFAULT_C1;
    uint64_t b1 = DEFAULT_B1;
    uint64_t s1 = DEFAULT_S1;
    uint64_t c2 = DEFAULT_C2;
    uint64_t b2 = DEFAULT_B2;
    uint64_t s2 = DEFAULT_S2;
    uint32_t k = DEFAULT_K;
    FILE* fin  = stdin;

    /* Read arguments */ 
    while(-1 != (opt = getopt(argc, argv, "c:b:s:C:B:S:k:i:h"))) {
        switch(opt) {
        case 'c':
            c1 = atoi(optarg);
            break;
        case 'b':
            b1 = atoi(optarg);
            break;
        case 's':
            s1 = atoi(optarg);
            break;
        case 'C':
            c2 = atoi(optarg);
            break;
        case 'B':
            b2 = atoi(optarg);
            break;
        case 'S':
            s2 = atoi(optarg);
            break;
        case 'k':
            k = atoi(optarg);
            break;
        case 'i':
            fin = fopen(optarg, "r");
            break;
        case 'h':
            /* Fall through */
        default:
            print_help_and_exit();
            break;
        }
    }

    printf("Cache Settings\n");
    printf("C1: %llu\n", c1);
    printf("B1: %llu\n", b1);
    printf("S1: %llu\n", s1);
    printf("C2: %llu\n", c2);
    printf("B2: %llu\n", b2);
    printf("S2: %llu\n", s2);
    printf("K: %u\n", k);
    printf("\n");

    assert(c2 >= c1);
    assert(b2 >= b1);
    assert(s2 >= s1);
    assert(k >= 0 && k <= 4);

    /* Setup the cache */
    setup_cache(c1, b1, s1, c2, b2, s2, k);

    /* Setup statistics */
    cache_stats_t stats;
    memset(&stats, 0, sizeof(cache_stats_t));

    /* Begin reading the file */ 
    char rw;
    uint64_t address;
    while (!feof(fin)) { 
        int ret = fscanf(fin, "%c %llx\n", &rw, &address); 
        if(ret == 2) {
            cache_access(rw, address, &stats); 
        }
    }

    complete_cache(&stats);

    print_statistics(&stats);

    return 0;
}

void print_statistics(cache_stats_t* p_stats) {
    printf("Cache Statistics\n");
    printf("L1 Accesses: %llu\n", p_stats->L1_accesses);
    printf("Reads Issued by CPU: %llu\n", p_stats->reads);
    printf("L1 Read misses: %llu\n", p_stats->L1_read_misses);
    printf("L2 Read misses: %llu\n", p_stats->L2_read_misses);
    printf("Writes Issued by CPU: %llu\n", p_stats->writes);
    printf("L1 Write misses: %llu\n", p_stats->L1_write_misses);
    printf("L2 Write misses: %llu\n", p_stats->L2_write_misses);
    printf("Write backs to Main Memory: %llu\n", p_stats->write_backs);
    printf("Prefetched blocks: %llu\n", p_stats->prefetched_blocks);
    printf("Successful prefetches: %llu\n", p_stats->successful_prefetches);
    printf("Average access time (AAT): %f\n", p_stats->avg_access_time);
}

