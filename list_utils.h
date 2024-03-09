#ifndef list_utils_H
#define list_utils_H

#include <stdlib.h>
#include "cache.h"
#include "micro.h"

int list_length(cache_block_t *ptr);
cache_block_t *list_pop(cache_block_t **ptr);
cache_block_t *list_shift(cache_block_t **ptr);
void list_push(cache_block_t **ptr, cache_block_t *e);
void list_append(cache_block_t **ptr, cache_block_t *e);
void list_split(cache_block_t *ptr, cache_block_t **chunks, int n);
cache_block_t *list_slice(cache_block_t **ptr, size_t s, size_t e);
cache_block_t *list_get(cache_block_t **ptr, size_t n);
void list_concat(cache_block_t **ptr, cache_block_t *chunk);
void list_from_chunks(cache_block_t **ptr, cache_block_t **chunks, int avoid, int len);
void list_set_id(cache_block_t *ptr, int id);
void print_list(cache_block_t *ptr);

//void initialize_random_list(cache_block_t *ptr, ul offset, ul sz, cache_block_t *base);
void initialize_list(cache_block_t *ptr, ul sz);
void pick_n_random_from_list(cache_block_t *set, unsigned long stride, unsigned long set_size,
			     unsigned long n);
void rearrange_list(cache_block_t **ptr, ul stride, ul sz, ul offset);
void generate_conflict_set(cache_block_t **ptr, cache_block_t **out, int rep, int threshold);

#endif /* list_utils_H */
