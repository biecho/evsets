#ifndef cache_H
#define cache_H

#include <stdlib.h>
#include <stdint.h>

#include "eviction.h"

void traverse_list_simple(cache_block_t *ptr);

int tests_avg(cache_block_t *ptr, char *victim, int rep, int threshold);

int calibrate(char *victim, struct eviction_config_t *conf);

#endif /* cache_H */
