#include "algorithms.h"
#include "list_utils.h"
#include "utils.h"
#include "public_structs.h"
#include <math.h>
#include <stdio.h>

#define MAX_REPS_BACK 100

extern struct config conf;

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
