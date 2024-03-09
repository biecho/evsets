#ifndef EVICTION_H
#define EVICTION_H

typedef struct cache_block_t {
	struct cache_block_t *next;
	struct cache_block_t *prev;
	int set;
	size_t delta;
	char pad[32]; // up to 64B
} cache_block_t;

struct eviction_config_t {
	int rounds, cal_rounds;
	int stride;
	int cache_size;
	int initial_set_size;
	int cache_way;
	int cache_slices;
	int threshold;
};

int find_eviction_set(char *pool, unsigned long pool_sz, char *victim, struct eviction_config_t conf,
		      cache_block_t **eviction_set);

#endif
