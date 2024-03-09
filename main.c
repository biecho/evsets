#include "cache.h"
#include "eviction.h"
#include "list_utils.h"

#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int
main()
{
	int seed = time(NULL);
	srand(seed);

	struct eviction_config_t conf = {
		.rounds = 10,
		.cal_rounds = 1000000,
		.stride = 4096,
		.cache_size = 12 << 20,
		.cache_way = 16,
		.cache_slices = 6,
		.initial_set_size = 4096,
	};

	char *buffer = (char *)mmap(NULL, 1 << 30, PROT_READ | PROT_WRITE,
				    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
	if (buffer == MAP_FAILED) {
		printf("[!] Error: Memory allocation failed\n");
		return 1;
	}

	// Consider the first 128MB as pool
	unsigned long long pool_sz = 256 << 20;
	char *pool = &buffer[0];
	char *victim = &buffer[(1 << 29)];

	cache_block_t *eviction_set = NULL;

	if (find_eviction_set(pool, pool_sz, victim, conf, &eviction_set) || !eviction_set) {
		printf("[-] Could not find all desired eviction sets.\n");
	}

	printf("[+] Found minimal eviction set for %p (length=%d): ", (void *)victim, list_length(eviction_set));
	print_list(eviction_set);

    if (tests_avg(eviction_set, victim, 10, 135)) {
		printf("1\n");
	} else {
		printf("0\n");
	}


	munmap(buffer, 1 << 30);
	return 0;
}
