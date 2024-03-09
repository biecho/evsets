#ifndef public_structs_H
#define public_structs_H

#define ALGORITHM_NAIVE 0
#define ALGORITHM_GROUP 1
#define ALGORITHM_BINARY 2
#define ALGORITHM_LINEAR 3
#define ALGORITHM_NAIVE_OPTIMISTIC 4

#define STRATEGY_SIMPLE 2

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
	void (*traverse)(cache_block_t *);
};

#endif /* public_structs_H */
