#include "cache.h"
#include "list_utils.h"
#include "public_structs.h"
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

#define MAX_REPS_BACK 100
#define MAX_REPS 50

void
shuffle(int *array, size_t n)
{
	size_t i;
	if (n > 1) {
		for (i = 0; i < n - 1; i++) {
			size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
			int t = array[j];
			array[j] = array[i];
			array[i] = t;
		}
	}
}

int
gt_eviction(cache_block_t **ptr, cache_block_t **can, char *victim, int cache_way, int rounds, int threshold)
{
	// Random chunk selection
	cache_block_t **chunks = (cache_block_t **)calloc(cache_way + 1, sizeof(cache_block_t *));
	if (!chunks) {
		return 1;
	}
	int *ichunks = (int *)calloc(cache_way + 1, sizeof(int)), i;
	if (!ichunks) {
		free(chunks);
		return 1;
	}

	int len = list_length(*ptr), cans = 0;

	// Calculate length: h = log(a/(a+1), a/n)
	double sz = (double)cache_way / len;
	double rate = (double)cache_way / (cache_way + 1);
	int h = ceil(log(sz) / log(rate)), l = 0;

	// Backtrack record
	cache_block_t **back =
		(cache_block_t **)calloc(h * 2, sizeof(cache_block_t *)); // TODO: check height bound
	if (!back) {
		free(chunks);
		free(ichunks);
		return 1;
	}

	int repeat = 0;
	do {
		for (i = 0; i < cache_way + 1; i++) {
			ichunks[i] = i;
		}
		shuffle(ichunks, cache_way + 1);

		// Reduce
		while (len > cache_way) {
			list_split(*ptr, chunks, cache_way + 1);
			int n = 0, ret = 0;

			// Try paths
			do {
				list_from_chunks(ptr, chunks, ichunks[n], cache_way + 1);
				n = n + 1;
				ret = tests_avg(*ptr, victim, rounds, threshold);
			} while (!ret && (n < cache_way + 1));

			// If find smaller eviction set remove chunk
			if (ret && n <= cache_way) {
				back[l] = chunks[ichunks[n - 1]]; // store ptr to discarded chunk
				cans += list_length(back[l]); // add length of removed chunk
				len = list_length(*ptr);

				printf("\tlvl=%d: eset=%d, removed=%d (%d)\n", l, len, cans, len + cans);

				l = l + 1; // go to next lvl
			}
			// Else, re-add last removed chunk and try again
			else if (l > 0) {
				list_concat(ptr, chunks[ichunks[n - 1]]); // recover last case
				l = l - 1;
				cans -= list_length(back[l]);
				list_concat(ptr, back[l]);
				back[l] = NULL;
				len = list_length(*ptr);
				goto mycont;
			} else {
				list_concat(ptr, chunks[ichunks[n - 1]]); // recover last case
				break;
			}
		}

		break;
	mycont:
		printf("\tbacktracking step\n");

	} while (l > 0 && repeat++ < MAX_REPS_BACK);

	// recover discarded elements
	for (i = 0; i < h * 2; i++) {
		list_concat(can, back[i]);
	}

	free(chunks);
	free(ichunks);
	free(back);

	int ret = 0;
	ret = tests_avg(*ptr, victim, rounds, threshold);
	if (ret) {
		if (len > cache_way) {
			return 1;
		}
	} else {
		return 1;
	}

	return 0;
}

int
find_evsets(char *pool, unsigned long pool_sz, char *victim, struct config conf)
{
	cache_block_t *set = NULL;
	cache_block_t *can = NULL;

	*victim = 0; // touch line

	clock_t tts, tte;
	int rep = 0;

	conf.threshold = calibrate(victim, &conf);
	printf("[+] Calibrated Threshold = %d\n", conf.threshold);

	if (conf.threshold < 0) {
		printf("[!] Error: calibration\n");
		return 1;
	}

	tts = clock();
pick:

	set = (cache_block_t *)&pool[0];
	initialize_list(set, pool_sz);

	int n = conf.initial_set_size;
	printf("[+] Pick %d random from list\n", n);
	pick_n_random_from_list(set, conf.stride, pool_sz, n);
	if (list_length(set) != n) {
		printf("[!] Error: broken list\n");
		return 1;
	}

	int ret = tests_avg(set, victim, conf.rounds, conf.threshold);

	if (victim && ret) {
		printf("[+] Initial candidate set evicted victim\n");
	} else {
		printf("[!] Error: invalid candidate set\n");
		if (rep < MAX_REPS) {
			rep++;
			goto pick;
		} else if (rep >= MAX_REPS) {
			printf("[!] Error: exceeded max repetitions\n");
		}
		return 1;
	}

	clock_t ts, te;

	int len = 0;
	// Iterate over all colors of conf.offset
	do {
		printf("[+] Created linked list structure (%d elements)\n", list_length(set));
		printf("[+] Starting group reduction...\n");

		ts = clock();
		ret = gt_eviction(&set, &can, victim, conf.cache_way, conf.rounds, conf.threshold);
		te = clock();

		tte = clock();

		len = list_length(set);
		if (ret) {
			printf("[!] Error: optimal eviction set not found (length=%d)\n", len);
		} else {
			printf("[+] Reduction time: %f seconds\n", ((double)(te - ts)) / CLOCKS_PER_SEC);
			printf("[+] Total execution time: %f seconds\n",
			       ((double)(tte - tts)) / CLOCKS_PER_SEC);

			// Re-Check that it's an optimal eviction set
			printf("[+] Found minimal eviction set for %p (length=%d): ", 
			       (void *)victim, len);
			print_list(set);
		}

		if (ret) {
			if (rep < MAX_REPS) {
				list_concat(&set, can);
				can = NULL;
				rep++;
				// select a new initial set
				printf("[!] Error: repeat, pick a new set\n");
				goto pick;
			} else {
				printf("[!] Error: exceeded max repetitions\n");
			}
		} else if (!ret) {
			rep = 0;
		} else {
			list_concat(&set, can);
			can = NULL;
		}

		// Remove rest of congruent elements
		set = can;
		printf("You do not want to find all, right? ----------------------\n");
		break;
	} while (rep < MAX_REPS);

	return ret;
}

int
main()
{
	int seed = time(NULL);
	srand(seed);

	struct config conf = {
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
	unsigned long long pool_sz = 128 << 20;
    char* pool = &buffer[0];
    char* victim = &buffer[1 << 29];

	if (find_evsets(pool, pool_sz, victim, conf)) {
		printf("[-] Could not find all desired eviction sets.\n");
	}

	munmap(buffer, 1 << 30);
	return 0;
}
