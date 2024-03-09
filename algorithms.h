#ifndef algorithms_H
#define algorithms_H

#include "cache.h"

int naive_eviction(cache_block_t **ptr, cache_block_t **can, char *victim);
int naive_eviction_optimistic(cache_block_t **ptr, cache_block_t **can, char *victim);
int gt_eviction(cache_block_t **ptr, cache_block_t **can, char *victim);
int gt_eviction_any(cache_block_t **ptr, cache_block_t **can);
int binary_eviction(cache_block_t **ptr, cache_block_t **can, char *victim);

#endif /* algorithms_H */
