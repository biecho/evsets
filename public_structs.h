#ifndef public_structs_H
#define public_structs_H

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
	int initial_set_size;
	int cache_way;
	int cache_slices;
	int threshold;
};

#endif /* public_structs_H */
