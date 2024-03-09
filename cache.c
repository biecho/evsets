#include "cache.h"
#include "eviction.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define LINE_BITS 6
#define PAGE_BITS 12
#define LINE_SIZE (1 << LINE_BITS)
#define PAGE_SIZE2 (1 << PAGE_BITS)

typedef unsigned long long int ul;

struct histogram {
	int val;
	int count;
};

void
hist_add(struct histogram *hist, int len, size_t val)
{
	// remove outliers
	int j = val;
	if (j < 800) // remove outliers
	{
		while (hist[j % len].val > 0 && hist[j % len].val != (int)val) {
			j++;
		}
		hist[j % len].val = val;
		hist[j % len].count++;
	}
}

float
hist_avg(struct histogram *hist, int len)
{
	float total = 0;
	int i = 0, n = 0;
	for (i = 0; i < len; i++) {
		if (hist[i].val > 0) {
			total += hist[i].val * hist[i].count;
			n += hist[i].count;
		}
	}
	return (float)(total / n);
}

int
hist_mode(struct histogram *hist, int len)
{
	int i, max = 0, mode = 0;
	for (i = 0; i < len; i++) {
		if (hist[i].count > max) {
			max = hist[i].count;
			mode = hist[i].val;
		}
	}
	return mode;
}

int
hist_min(struct histogram *hist, int len)
{
	int i, min = 99999;
	for (i = 0; i < len; i++) {
		if (hist[i].count > 0 && hist[i].val < min) {
			min = hist[i].val;
		}
	}
	return min;
}

int
hist_max(struct histogram *hist, int len)
{
	int i, max = 0;
	for (i = 0; i < len; i++) {
		if (hist[i].count > 0 && hist[i].val > max) {
			max = hist[i].val;
		}
	}
	return max;
}

double
hist_variance(struct histogram *hist, int len, int mean)
{
	int i, count = 0;
	double sum = 0;
	for (i = 0; i < len; i++) {
		if (hist[i].count > 0) {
			sum += pow((double)(hist[i].val - mean), 2.0) * hist[i].count;
			count += hist[i].count;
		}
	}
	return sum / count;
}

double
hist_std(struct histogram *hist, int len, int mean)
{
	return sqrt(hist_variance(hist, len, mean));
}

// count number of misses
int
hist_q(struct histogram *hist, int len, int threshold)
{
	int i = 0, count = 0;
	for (i = 0; i < len; i++) {
		if (hist[i].count > 0 && hist[i].val > threshold) {
			count += hist[i].count;
		}
	}
	return count;
}

inline void
flush(void *p)
{
	__asm__ volatile("clflush 0(%0)" : : "c"(p) : "rax");
}

inline uint64_t
rdtscfence()
{
	uint64_t a, d;
	__asm__ volatile("lfence");
	__asm__ volatile("rdtsc" : "=a"(a), "=d"(d) : :);
	__asm__ volatile("lfence");
	return ((d << 32) | a);
}

inline void
maccess(void *p)
{
	__asm__ volatile("movq (%0), %%rax\n" : : "c"(p) : "rax");
}

inline void
traverse_list_simple(cache_block_t *set)
{
	while (set) {
		maccess(set);
		set = set->next;
	}
}

static int
test_set(cache_block_t *set, char *victim)
{
	maccess(victim);
	maccess(victim);
	maccess(victim);
	maccess(victim);

	traverse_list_simple(set);

	maccess(victim + 222); // page walk

	size_t delta, time;
	time = rdtscfence();
	maccess(victim);
	delta = rdtscfence() - time;
	return delta;
}

/**
 * 
 * @return 1 if average delta exceeds threshold, indicating performance issue; otherwise, 0.
*/
int
tests_avg(cache_block_t *set, char *victim, int rep, int threshold)
{
	int i = 0, avg = 0, delta = 0;
	cache_block_t *vic = (cache_block_t *)victim;
	vic->delta = 0;
	for (i = 0; i < rep; i++) {
		delta = test_set(set, victim);
		if (delta < 800) {
			// Otherwise, we probably have a noisy measurement
			vic->delta += delta;
		}
	}
	avg = (float)vic->delta / rep;
	return avg > threshold;
}

int
calibrate(char *victim, struct eviction_config_t *conf)
{
	size_t delta, time, t_flushed, t_unflushed;
	struct histogram *flushed, *unflushed;
	int i, ret, hsz = conf->cal_rounds * 100;

	flushed = (struct histogram *)calloc(hsz, sizeof(struct histogram));
	unflushed = (struct histogram *)calloc(hsz, sizeof(struct histogram));

	if (flushed == NULL || unflushed == NULL) {
		return -1;
	}

	for (i = 0; i < conf->cal_rounds; i++) {
		maccess(victim);
		maccess(victim);
		maccess(victim);
		maccess(victim);

		maccess(victim + 222); // page walk

		time = rdtscfence();
		maccess(victim);
		delta = rdtscfence() - time;
		hist_add(unflushed, hsz, delta);
	}
	t_unflushed = hist_avg(unflushed, hsz);

	for (i = 0; i < conf->cal_rounds; i++) {
		maccess(victim); // page walk
		flush(victim);

		time = rdtscfence();
		maccess(victim);
		delta = rdtscfence() - time;
		hist_add(flushed, hsz, delta);
	}
	t_flushed = hist_avg(flushed, hsz);

	ret = hist_min(flushed, hsz);

	printf("\tflushed: min %d, mode %d, avg %f, max %d, std %.02f, q %d (%.02f)\n",
	       hist_min(flushed, hsz), hist_mode(flushed, hsz), hist_avg(flushed, hsz),
	       hist_max(flushed, hsz), hist_std(flushed, hsz, hist_avg(flushed, hsz)),
	       hist_q(flushed, hsz, ret), (double)hist_q(flushed, hsz, ret) / conf->cal_rounds);
	printf("\tunflushed: min %d, mode %d, avg %f, max %d, std %.02f, q %d (%.02f)\n",
	       hist_min(unflushed, hsz), hist_mode(unflushed, hsz), hist_avg(unflushed, hsz),
	       hist_max(unflushed, hsz), hist_std(unflushed, hsz, hist_avg(unflushed, hsz)),
	       hist_q(unflushed, hsz, ret), (double)hist_q(unflushed, hsz, ret) / conf->cal_rounds);

	free(unflushed);
	free(flushed);

	if (t_flushed < t_unflushed) {
		return -1;
	} else {
		return (t_flushed + t_unflushed * 2) / 3;
	}
}
