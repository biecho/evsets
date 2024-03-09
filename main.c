#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#ifdef __MACH__
#include <mach/vm_statistics.h>
#endif

#include "cache.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>

#include "list_utils.h"
#include "hist_utils.h"
#include "utils.h"
#include "cache.h"
#include "micro.h"
#include "list_utils.h"
#include "utils.h"
#include "public_structs.h"
#include <math.h>
#include <stdio.h>

struct config configuration = {
	.rounds = 10,
	.cal_rounds = 1000000,
	.stride = 4096,
	.cache_size = 6 << 20,
	.cache_way = 12,
	.cache_slices = 4,
	.algorithm = ALGORITHM_GROUP,
	.strategy = 2,
	.offset = 0,
	.con = 0,
	.noncon = 0,
	.ratio = -1.0,
	.buffer_size = 3072,
	.flags = FLAG_CALIBRATE,
};

#define MAX_REPS_BACK 100

#define MAX_REPS 50

struct config conf;

static cache_block_t **evsets = NULL;
static int num_evsets = 0;
static int colors = 0;
static char *probe = NULL;
static char *pool = NULL;
static ul pool_sz = 0;
static ul sz = 0;

int
init_evsets(struct config *conf_ptr)
{
	// save config
	memcpy(&conf, conf_ptr, sizeof(struct config));
	printf("[+] Configuration loaded\n");

	sz = conf.buffer_size * conf.stride;
	pool_sz = 128 << 20; // 128MB
	printf("[+] Total buffer size required: %llu bytes\n", sz);
	printf("[+] Memory pool size allocated: %llu bytes (128MB)\n", pool_sz);

	if (sz > pool_sz) {
		printf("[!] Error: Buffer size %llu exceeds allocated pool size %llu\n", sz, pool_sz);
		return 1;
	}

	printf("[*] HugePages used if available\n");
	pool = (char *)mmap(NULL, pool_sz, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
	probe = (char *)mmap(NULL, pool_sz, PROT_READ | PROT_WRITE,
					MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);

	if (pool == MAP_FAILED || probe == MAP_FAILED) {
		printf("[!] Error: Memory allocation failed\n");
		return 1;
	}

	printf("[+] Memory allocated successfully: Pool at %p, Probe at %p\n", (void *)pool, (void *)probe);
	printf("[+] %llu MB buffer allocated at %p (%llu blocks)\n", sz >> 20,
	       (void *)&pool[conf.offset << 6], sz / sizeof(cache_block_t));

	if (conf.stride < 64 || conf.stride % 64 != 0) {
		printf("[!] Error: Invalid stride %d. Stride must be a multiple of 64 and >= 64.\n",
		       conf.stride);
		goto err;
	}

	// Set eviction strategy
	printf("[*] Setting eviction strategy based on configuration\n");
	switch (conf.strategy) {
	case STRATEGY_SIMPLE:
	default:
		printf("[+] Using simple list traversal strategy (default)\n");
		conf.traverse = &traverse_list_simple;
		break;
	}

	colors = conf.cache_size / conf.cache_way / conf.stride;
	printf("[+] conf.cache_size = %d, conf.cache_way = %d, conf.stride = %d, colors = %d\n",
	       conf.cache_size, conf.cache_way, conf.stride, colors);
	evsets = calloc(colors, sizeof(cache_block_t *));
	if (!evsets) {
		printf("[!] Error: Failed to allocate memory for eviction sets\n");
		goto err;
	}
	printf("[+] Eviction sets allocated for %d colors\n", colors);

	return 0;

err:
	munmap(probe, pool_sz);
	munmap(pool, pool_sz);
#ifdef THREAD_COUNTER
	printf("[*] Cleaning up thread counter\n");
	destroy_counter();
#endif /* THREAD_COUNTER */
	return 1;
}

void
close_evsets()
{
	free(evsets);
	munmap(probe, pool_sz);
	munmap(pool, pool_sz);
#ifdef THREAD_COUNTER
	destroy_counter();
#endif /* THREAD_COUNTER */
}

int
get_num_evsets()
{
	return num_evsets;
}

cache_block_t *
get_evset(int id)
{
	if (id >= num_evsets) {
		return NULL;
	}

	return evsets[id];
}

int
gt_eviction(cache_block_t **ptr, cache_block_t **can, char *victim)
{
	// Random chunk selection
	cache_block_t **chunks = (cache_block_t **)calloc(conf.cache_way + 1, sizeof(cache_block_t *));
	if (!chunks) {
		return 1;
	}
	int *ichunks = (int *)calloc(conf.cache_way + 1, sizeof(int)), i;
	if (!ichunks) {
		free(chunks);
		return 1;
	}

	int len = list_length(*ptr), cans = 0;

	// Calculate length: h = log(a/(a+1), a/n)
	double sz = (double)conf.cache_way / len;
	double rate = (double)conf.cache_way / (conf.cache_way + 1);
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
		for (i = 0; i < conf.cache_way + 1; i++) {
			ichunks[i] = i;
		}
		shuffle(ichunks, conf.cache_way + 1);

		// Reduce
		while (len > conf.cache_way) {
			list_split(*ptr, chunks, conf.cache_way + 1);
			int n = 0, ret = 0;

			// Try paths
			do {
				list_from_chunks(ptr, chunks, ichunks[n], conf.cache_way + 1);
				n = n + 1;
				if (conf.ratio > 0.0) {
					ret = tests(*ptr, victim, conf.rounds, conf.threshold, conf.ratio,
						    conf.traverse);
				} else {
					ret = tests_avg(*ptr, victim, conf.rounds, conf.threshold,
							conf.traverse);
				}
			} while (!ret && (n < conf.cache_way + 1));

			// If find smaller eviction set remove chunk
			if (ret && n <= conf.cache_way) {
				back[l] = chunks[ichunks[n - 1]]; // store ptr to discarded chunk
				cans += list_length(back[l]); // add length of removed chunk
				len = list_length(*ptr);

				if (conf.flags & FLAG_VERBOSE) {
					printf("\tlvl=%d: eset=%d, removed=%d (%d)\n", l, len, cans,
					       len + cans);
				}

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
		if (conf.flags & FLAG_VERBOSE) {
			printf("\tbacktracking step\n");
		}

	} while (l > 0 && repeat++ < MAX_REPS_BACK && (conf.flags & FLAG_BACKTRACKING));

	// recover discarded elements
	for (i = 0; i < h * 2; i++) {
		list_concat(can, back[i]);
	}

	free(chunks);
	free(ichunks);
	free(back);

	int ret = 0;
	if (conf.ratio > 0.0) {
		ret = tests(*ptr, victim, conf.rounds, conf.threshold, conf.ratio, conf.traverse);
	} else {
		ret = tests_avg(*ptr, victim, conf.rounds, conf.threshold, conf.traverse);
	}
	if (ret) {
		if (len > conf.cache_way) {
			return 1;
		}
	} else {
		return 1;
	}

	return 0;
}

int
find_evsets()
{
	char *victim = NULL;
	cache_block_t *ptr = NULL;
	cache_block_t *can = NULL;

	victim = &probe[conf.offset << 6];
	*victim = 0; // touch line

	int seed = time(NULL);
	srand(seed);

	if (conf.flags & FLAG_CALIBRATE) {
		conf.threshold = calibrate(victim, &conf);
		printf("[+] Calibrated Threshold = %d\n", conf.threshold);
	} else {
		printf("[+] Default Threshold = %d\n", conf.threshold);
	}

	if (conf.threshold < 0) {
		printf("[!] Error: calibration\n");
		return 1;
	}

	if (conf.algorithm == ALGORITHM_LINEAR) {
		victim = NULL;
	}

	clock_t tts, tte;
	int rep = 0;
	tts = clock();
pick:

	ptr = (cache_block_t *)&pool[conf.offset << 6];
	initialize_list(ptr, pool_sz, conf.offset);

	// Conflict set incompatible with ANY case (don't needed)
	if ((conf.flags & FLAG_CONFLICTSET) && (conf.algorithm != ALGORITHM_LINEAR)) {
		pick_n_random_from_list(ptr, conf.stride, pool_sz, conf.offset, conf.buffer_size);
		generate_conflict_set(&ptr, &can);
		printf("[+] Compute conflict set: %d\n", list_length(can));
		victim = (char *)ptr;
		ptr = can; // new conflict set
		while (victim &&
		       !tests(ptr, victim, conf.rounds, conf.threshold, conf.ratio, conf.traverse)) {
			victim = (char *)(((cache_block_t *)victim)->next);
		}
		can = NULL;
	} else {
		pick_n_random_from_list(ptr, conf.stride, pool_sz, conf.offset, conf.buffer_size);
		if (list_length(ptr) != conf.buffer_size) {
			printf("[!] Error: broken list\n");
			return 1;
		}
	}

	int ret = 0;
	if (conf.flags & FLAG_DEBUG) {
		conf.flags |= FLAG_VERIFY;
		conf.flags &= ~(FLAG_FINDALLCOLORS | FLAG_FINDALLCONGRUENT);
		printf("[+] Filter: %d congruent, %d non-congruent addresses\n", conf.con, conf.noncon);
		ret = filter(&ptr, victim, conf.con, conf.noncon, &conf);
		if (ret && (conf.flags & FLAG_RETRY)) {
			return 1;
		}
	}

	if (conf.algorithm == ALGORITHM_LINEAR) {
		ret = test_and_time(ptr, conf.rounds, conf.threshold, conf.cache_way, conf.traverse);
	} else if (victim) {
		if (conf.ratio > 0.0) {
			ret = tests(ptr, victim, conf.rounds, conf.threshold, conf.ratio, conf.traverse);
		} else {
			ret = tests_avg(ptr, victim, conf.rounds, conf.threshold, conf.traverse);
		}
	}
	if ((victim || conf.algorithm == ALGORITHM_LINEAR) && ret) {
		printf("[+] Initial candidate set evicted victim\n");
		// rep = 0;
	} else {
		printf("[!] Error: invalid candidate set\n");
		if ((conf.flags & FLAG_RETRY) && rep < MAX_REPS) {
			rep++;
			goto pick;
		} else if (rep >= MAX_REPS) {
			printf("[!] Error: exceeded max repetitions\n");
		}
		if (conf.flags & FLAG_VERIFY) {
			recheck(ptr, victim, true, &conf);
		}
		return 1;
	}

	clock_t ts, te;

	int len = 0;
	int id = num_evsets;
	// Iterate over all colors of conf.offset
	do {
		printf("[+] Created linked list structure (%d elements)\n", list_length(ptr));

		// Search
		switch (conf.algorithm) {
		case ALGORITHM_GROUP:
			printf("[+] Starting group reduction...\n");
			ts = clock();
			ret = gt_eviction(&ptr, &can, victim);
			te = clock();
			break;
		}

		tte = clock();

		len = list_length(ptr);
		if (ret) {
			printf("[!] Error: optimal eviction set not found (length=%d)\n", len);
		} else {
			printf("[+] Reduction time: %f seconds\n", ((double)(te - ts)) / CLOCKS_PER_SEC);
			printf("[+] Total execution time: %f seconds\n",
			       ((double)(tte - tts)) / CLOCKS_PER_SEC);

			// Re-Check that it's an optimal eviction set
			if (conf.algorithm != ALGORITHM_LINEAR) {
				printf("[+] (ID=%d) Found minimal eviction set for %p (length=%d): ", id,
				       (void *)victim, len);
				print_list(ptr);
			} else {
				printf("[+] (ID=%d) Found a minimal eviction set (length=%d): ", id, len);
				print_list(ptr);
			}
			evsets[id] = ptr;
			num_evsets += 1;
		}

		if (conf.flags & FLAG_VERIFY) {
			recheck(ptr, victim, ret, &conf);
		}

		if (ret && (conf.flags & FLAG_RETRY)) {
			if (rep < MAX_REPS) {
				list_concat(&ptr, can);
				can = NULL;
				rep++;
				if (!(conf.flags & FLAG_CONFLICTSET) && !(conf.flags & FLAG_FINDALLCOLORS)) {
					// select a new initial set
					printf("[!] Error: repeat, pick a new set\n");
					goto pick;
				} else {
					// reshuffle list or change victim?
					printf("[!] Error: try new victim\n");
					goto next;
					// continue;
				}
			} else {
				printf("[!] Error: exceeded max repetitions\n");
			}
		} else if (!ret) {
			rep = 0;
		} else {
			list_concat(&ptr, can);
			can = NULL;
		}

		// Remove rest of congruent elements
		list_set_id(evsets[id], id);
		ptr = can;
		if (conf.flags & FLAG_FINDALLCONGRUENT) {
			cache_block_t *e = NULL, *head = NULL, *done = NULL, *tmp = NULL;
			int count = 0, t = 0;
			while (ptr) {
				e = list_pop(&ptr);
				if (conf.ratio > 0.0) {
					t = tests(evsets[id], (char *)e, conf.rounds, conf.threshold,
						  conf.ratio, conf.traverse);
				} else {
					t = tests_avg(evsets[id], (char *)e, conf.rounds, conf.threshold,
						      conf.traverse);
				}
				if (t) {
					// create list of congruents
					e->set = id;
					count++;
					list_push(&head, e);
				} else {
					list_push(&done, e);
				}
			}
			if (tmp) {
				tmp->next = NULL;
			}
			printf("[+] Found %d more congruent elements from set id=%d\n", count, id);
			list_concat(&evsets[id], head);
			ptr = done;
		}

		if (!(conf.flags & FLAG_FINDALLCOLORS)) {
			break;
		}
		printf("----------------------\n");
		id = id + 1;
		if (id == colors || !ptr || ((conf.flags & FLAG_CONFLICTSET) && !victim) ||
		    (!(conf.flags & FLAG_CONFLICTSET) && victim >= probe + pool_sz - conf.stride)) {
			printf("[+] Found all eviction sets in buffer\n");
			break;
		}

	next:
		// Find victim for different color. Only for specific algorithms.
		if (conf.algorithm != ALGORITHM_LINEAR) {
			int s = 0, ret = 0, ret2 = 0;
			do {
				if (!(conf.flags & FLAG_CONFLICTSET)) {
					victim += conf.stride;
					*victim = 0;
				} else {
					victim = (char *)((cache_block_t *)victim)->next;
				}

				// Check again. Better reorganize this mess.
				if (((conf.flags & FLAG_CONFLICTSET) && !victim) ||
				    (!(conf.flags & FLAG_CONFLICTSET) &&
				     victim >= probe + pool_sz - conf.stride)) {
					break;
				}

				// New victim is not evicted by previous eviction sets
				for (ret = 0, s = 0; s < id && !ret; s++) {
					if (conf.ratio > 0.0) {
						ret = tests(evsets[s], victim, conf.rounds, conf.threshold,
							    conf.ratio, conf.traverse);
					} else {
						ret = tests_avg(evsets[s], victim, conf.rounds,
								conf.threshold, conf.traverse);
					}
				}
				if (!ret) {
					// Rest of initial eviction set can evict victim
					if (conf.ratio > 0.0) {
						ret2 = tests(ptr, victim, conf.rounds, conf.threshold,
							     conf.ratio, conf.traverse);
					} else {
						ret2 = tests_avg(ptr, victim, conf.rounds, conf.threshold,
								 conf.traverse);
					}
				}
			} while ((list_length(ptr) > conf.cache_way) && !ret2 &&
				 (((conf.flags & FLAG_CONFLICTSET) && victim) ||
				  (!(conf.flags & FLAG_CONFLICTSET) &&
				   (victim < (probe + pool_sz - conf.stride)))));

			if (ret2) {
				printf("[+] Found new victim %p\n", (void *)victim);
			} else {
				printf("[!] Error: couldn't find more victims\n");
				return 1;
			}
		}

		can = NULL;

	} while (((conf.flags & FLAG_FINDALLCOLORS) && id < colors) ||
		 ((conf.flags & FLAG_RETRY) && rep < MAX_REPS));

	return ret;
}

void
usage(char *name)
{
	printf("[?] Usage: %s [flags] [params]\n\n"
	       "\tFlags:\n"
	       "\t\t--nohugepages\n"
	       "\t\t--retry\t(complete repetitions)\n"
	       "\t\t--backtracking\n"
	       "\t\t--verbose\n"
	       "\t\t--verify\t(requires root)\n"
	       "\t\t--ignoreslice\t(unknown slicing function)\n"
	       "\t\t--findallcolors\n"
	       "\t\t--findallcongruent\n"
	       "\t\t--conflictset\n"
	       "\tParams:\n"
	       "\t\t-b N\t\tnumber of lines in initial buffer (default: 3072)\n"
	       "\t\t-t N\t\tthreshold in cycles (default: calibrates)\n"
	       "\t\t-c N\t\tcache size in MB (default: 6)\n"
	       "\t\t-s N\t\tnumber of cache slices (default: 4)\n"
	       "\t\t-n N\t\tcache associativity (default: 12)\n"
	       "\t\t-o N\t\tstride for blocks in bytes (default: 4096)\n"
	       "\t\t-a n|o|g|l\tsearch algorithm (default: 'g')\n"
	       "\t\t-e 0|1|2|3|4\teviction strategy: 0-haswell, 1-skylake, 2-simple (default: 2)\n"
	       "\t\t-C N\t\tpage offset (default: 0)\n"
	       "\t\t-r N\t\tnumer of rounds per test (default: 10)\n"
	       "\t\t-q N\t\tratio of success for passing a test (default: disabled)\n"
	       "\t\t-h\t\tshow this help\n\n"
	       "\tExample:\n\t\t%s -b 3000 -c 6 -s 8 -a g -n 12 -o 4096 -e 2 -C 0 -t 85 --verbose --retry --backtracking\n"
	       "\n",
	       name, name);
}

#define KEY 0xd34dc0d3

int
main(int argc, char **argv)
{
	int option = 0, option_index = 0;

	static struct option long_options[] = { { "nohugepages", no_argument, 0, FLAG_NOHUGEPAGES ^ KEY },
						{ "retry", no_argument, 0, FLAG_RETRY ^ KEY },
						{ "backtracking", no_argument, 0, FLAG_BACKTRACKING ^ KEY },
						{ "verbose", no_argument, 0, FLAG_VERBOSE ^ KEY },
						{ "verify", no_argument, 0, FLAG_VERIFY ^ KEY },
						{ "debug", no_argument, 0, FLAG_DEBUG ^ KEY },
						{ "ignoreslice", no_argument, 0, FLAG_IGNORESLICE ^ KEY },
						{ "findallcolors", no_argument, 0, FLAG_FINDALLCOLORS ^ KEY },
						{ "findallcongruent", no_argument, 0,
						  FLAG_FINDALLCONGRUENT ^ KEY },
						{ "conflictset", no_argument, 0, FLAG_CONFLICTSET ^ KEY },
						{ "buffer-size", no_argument, 0, 'b' },
						{ "threshold", no_argument, 0, 't' },
						{ "ratio", no_argument, 0, 'q' },
						{ "cache-size", no_argument, 0, 'c' },
						{ "cache-slices", no_argument, 0, 's' },
						{ "associativity", no_argument, 0, 'n' },
						{ "stride", no_argument, 0, 'o' },
						{ "algorithm", no_argument, 0, 'a' },
						{ "strategy", no_argument, 0, 'e' },
						{ "offset", no_argument, 0, 'C' },
						{ "rounds", no_argument, 0, 'r' },
						{ "help", no_argument, 0, 'h' },
						{ "con", no_argument, 0, 'x' },
						{ "noncon", no_argument, 0, 'y' },
						{ 0, 0, 0, 0 } };

	while ((option = getopt_long(argc, argv, "hb:t:q:c:s:n:o:a:e:r:C:x:y:", long_options,
				     &option_index)) != -1) {
		switch (option) {
		case 0:
			break;
		case 'b':
			configuration.buffer_size = atoi(optarg);
			break;
		case 't':
			configuration.flags &= ~FLAG_CALIBRATE;
			configuration.threshold = atoi(optarg);
			break;
		case 'q':
			configuration.ratio = atof(optarg);
			break;
		case 'c':
			configuration.cache_size = atoi(optarg) << 20;
			break;
		case 's':
			configuration.cache_slices = atoi(optarg);
			if (configuration.cache_slices < 1 || configuration.cache_slices % 2) {
				printf("[-] Invalid number of slices\n");
				configuration.cache_slices = 1;
			} else if (configuration.cache_slices > 8) {
				printf("[-] No support for more than 8 slices\n");
				configuration.cache_slices = 8;
			}
			break;
		case 'n':
			configuration.cache_way = atoi(optarg);
			if (configuration.cache_way < 1) {
				configuration.cache_way = 1;
			}
			break;
		case 'o':
			configuration.stride = atoi(optarg);
			break;
		case 'a':
			if (strncmp(optarg, "g", strlen(optarg)) == 0) {
				configuration.algorithm = ALGORITHM_GROUP;
			} else if (strncmp(optarg, "b", strlen(optarg)) == 0) {
				configuration.algorithm = ALGORITHM_BINARY;
			} else if (strncmp(optarg, "l", strlen(optarg)) == 0) {
				configuration.algorithm = ALGORITHM_LINEAR;
			} else if (strncmp(optarg, "n", strlen(optarg)) == 0) {
				configuration.algorithm = ALGORITHM_NAIVE;
			} else if (strncmp(optarg, "o", strlen(optarg)) == 0) {
				configuration.algorithm = ALGORITHM_NAIVE_OPTIMISTIC;
			}
			break;
		case 'e':
			configuration.strategy = atoi(optarg);
			break;
		case 'C':
			configuration.offset = atoi(optarg);
			break;
		case 'r':
			configuration.rounds = atoi(optarg);
			break;
		case 'x':
			configuration.con = atoi(optarg);
			if (configuration.con < 0) {
				configuration.con = 0;
			}
			break;
		case 'y':
			configuration.noncon = atoi(optarg);
			if (configuration.noncon < 0) {
				configuration.noncon = 0;
			}
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			/* encoded flag to avoid collision with ascii letters */
			configuration.flags |= (option ^ KEY);
		}
	}

	if (init_evsets(&configuration)) {
		printf("[-] Initializing evsets library failed.\n");
		return 1;
	}

	if (find_evsets()) {
		printf("[-] Could not find all desired eviction sets.\n");
	}

	close_evsets();

	return 0;
}
