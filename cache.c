#include "cache.h"
#include "micro.h"
#include "hist_utils.h"
#include "public_structs.h"

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
