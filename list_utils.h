#ifndef list_utils_H
#define list_utils_H

#include <stdlib.h>
#include "cache.h"
#include "eviction.h"

int list_length(cache_block_t *ptr);
void list_split(cache_block_t *ptr, cache_block_t **chunks, int n);
void list_concat(cache_block_t **ptr, cache_block_t *chunk);
void list_from_chunks(cache_block_t **ptr, cache_block_t **chunks, int avoid, int len);
void print_list(cache_block_t *ptr);

void initialize_list(cache_block_t *ptr, unsigned long sz);
void pick_n_random_from_list(cache_block_t *set, unsigned long stride, unsigned long set_size,
			     unsigned long n);

#endif /* list_utils_H */
