#ifndef public_structs_H
#define public_structs_H

#define ALGORITHM_NAIVE 0
#define ALGORITHM_GROUP 1
#define ALGORITHM_BINARY 2
#define ALGORITHM_LINEAR 3
#define ALGORITHM_NAIVE_OPTIMISTIC 4

#define STRATEGY_SIMPLE 2

#define FLAG_VERBOSE (1 << 0)
#define FLAG_NOHUGEPAGES (1 << 1)
#define FLAG_CALIBRATE (1 << 2)
#define FLAG_RETRY (1 << 3)
#define FLAG_BACKTRACKING (1 << 4)
#define FLAG_IGNORESLICE (1 << 5)
#define FLAG_FINDALLCOLORS (1 << 6)
#define FLAG_FINDALLCONGRUENT (1 << 7)
#define FLAG_VERIFY (1 << 8)
#define FLAG_DEBUG (1 << 9)
#define FLAG_CONFLICTSET (1 << 10)

typedef struct cache_block_t {
	struct cache_block_t *next;
	struct cache_block_t *prev;
	int set;
	size_t delta;
	char pad[32]; // up to 64B
} cache_block_t;

struct config {
	int rounds, cal_rounds;
	int stride;
	int cache_size;
	int buffer_size;
	int cache_way;
	int cache_slices;
	int threshold;
	int algorithm;
	int strategy;
	int offset;
	int con, noncon; // only for debug
	void (*traverse)(cache_block_t *);
	int flags;
};

#endif /* public_structs_H */
