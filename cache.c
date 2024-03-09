#include "cache.h"
#include "micro.h"
#include "hist_utils.h"
#include "public_structs.h"

inline void
traverse_list_simple(cache_block_t *ptr)
{
	while (ptr) {
		maccess(ptr);
		ptr = ptr->next;
	}
}

inline void
traverse_list_time(cache_block_t *ptr, void (*trav)(cache_block_t *))
{
	size_t time;
	trav(ptr);
	while (ptr) {
		//		time = rdtsc();
		time = rdtscfence();
		maccess(ptr);
		ptr->delta += rdtscfence() - time;
		//		ptr->delta += rdtscp() - time;
		ptr = ptr->next;
	}
}

int
test_set(cache_block_t *ptr, char *victim, void (*trav)(cache_block_t *))
{
	maccess(victim);
	maccess(victim);
	maccess(victim);
	maccess(victim);

	trav(ptr);

	maccess(victim + 222); // page walk

	size_t delta, time;
#ifndef THREAD_COUNTER
	//	time = rdtsc();
	time = rdtscfence();
	maccess(victim);
	//	delta = rdtscp() - time;
	delta = rdtscfence() - time;
#else
	time = clock_thread();
	maccess(victim);
	delta = clock_thread() - time;
#endif
	return delta;
}

int
tests_avg(cache_block_t *ptr, char *victim, int rep, int threshold, void (*trav)(cache_block_t *))
{
	int i = 0, ret = 0, delta = 0;
	cache_block_t *vic = (cache_block_t *)victim;
	vic->delta = 0;
	for (i = 0; i < rep; i++) {
		delta = test_set(ptr, victim, trav);
		if (delta < 800)
			vic->delta += delta;
	}
	ret = (float)vic->delta / rep;
	return ret > threshold;
}

int
calibrate(char *victim, struct config *conf)
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

#ifndef THREAD_COUNTER
		//		time = rdtsc();
		time = rdtscfence();
		maccess(victim);
		//		delta = rdtscp() - time;
		delta = rdtscfence() - time;
#else
		time = clock_thread();
		maccess(victim);
		delta = clock_thread() - time;
#endif
		hist_add(unflushed, hsz, delta);
	}
	t_unflushed = hist_avg(unflushed, hsz);

	for (i = 0; i < conf->cal_rounds; i++) {
		maccess(victim); // page walk
		flush(victim);

#ifndef THREAD_COUNTER
		//		time = rdtsc();
		time = rdtscfence();
		maccess(victim);
		//		delta = rdtscp() - time;
		delta = rdtscfence() - time;
#else
		time = clock_thread();
		maccess(victim);
		delta = clock_thread() - time;
#endif
		hist_add(flushed, hsz, delta);
	}
	t_flushed = hist_avg(flushed, hsz);

	ret = hist_min(flushed, hsz);

	if (conf->flags & FLAG_VERBOSE) {
		printf("\tflushed: min %d, mode %d, avg %f, max %d, std %.02f, q %d (%.02f)\n",
		       hist_min(flushed, hsz), hist_mode(flushed, hsz), hist_avg(flushed, hsz),
		       hist_max(flushed, hsz), hist_std(flushed, hsz, hist_avg(flushed, hsz)),
		       hist_q(flushed, hsz, ret), (double)hist_q(flushed, hsz, ret) / conf->cal_rounds);
		printf("\tunflushed: min %d, mode %d, avg %f, max %d, std %.02f, q %d (%.02f)\n",
		       hist_min(unflushed, hsz), hist_mode(unflushed, hsz), hist_avg(unflushed, hsz),
		       hist_max(unflushed, hsz), hist_std(unflushed, hsz, hist_avg(unflushed, hsz)),
		       hist_q(unflushed, hsz, ret), (double)hist_q(unflushed, hsz, ret) / conf->cal_rounds);
	}

	free(unflushed);
	free(flushed);

	if (t_flushed < t_unflushed) {
		return -1;
	} else {
		return (t_flushed + t_unflushed * 2) / 3;
	}
}
