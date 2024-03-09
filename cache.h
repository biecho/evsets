#ifndef cache_H
#define cache_H

#include <stdlib.h>
#include <stdint.h>

#ifdef THREAD_COUNTER
#include <pthread.h>
#endif

#include "private_structs.h"

#ifdef THREAD_COUNTER
static pthread_t thread;
static struct params_t params;

void *counter_thread();
static inline uint64_t clock_thread();
int create_counter();
void destroy_counter();
#endif /* THREAD_COUNTER */

void traverse_list_simple(cache_block_t *ptr);
void traverse_list_rrip(cache_block_t *ptr);
void traverse_list_to_n(cache_block_t *ptr, int n);
void traverse_list_time(cache_block_t *ptr, void (*trav)(cache_block_t *));

int test_set(cache_block_t *ptr, char *victim, void (*trav)(cache_block_t *));
int tests(cache_block_t *ptr, char *victim, int rep, int threshold, float ratio,
	  void (*trav)(cache_block_t *));
int tests_avg(cache_block_t *ptr, char *victim, int rep, int threshold, void (*trav)(cache_block_t *));
int test_and_time(cache_block_t *ptr, int rep, int threshold, int ways, void (*trav)(cache_block_t *));

int calibrate(char *victim, struct config *conf);

#endif /* cache_H */
