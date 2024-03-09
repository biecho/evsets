#include "list_utils.h"
#include <stdio.h>
#include <stdlib.h>

int
list_length(cache_block_t *ptr)
{
	int l = 0;
	while (ptr) {
		l = l + 1;
		ptr = ptr->next;
	}
	return l;
}

/* add element to the head of the list */
void
list_push(cache_block_t **ptr, cache_block_t *e)
{
	if (!e) {
		return;
	}
	e->prev = NULL;
	e->next = *ptr;
	if (*ptr) {
		(*ptr)->prev = e;
	}
	*ptr = e;
}

/* add element to the end of the list */
void
list_append(cache_block_t **ptr, cache_block_t *e)
{
	cache_block_t *tmp = *ptr;
	if (!e) {
		return;
	}
	if (!tmp) {
		*ptr = e;
		return;
	}
	while (tmp->next) {
		tmp = tmp->next;
	}
	tmp->next = e;
	e->prev = tmp;
	e->next = NULL;
}

/* remove and return last element of list */
cache_block_t *
list_shift(cache_block_t **ptr)
{
	cache_block_t *tmp = (ptr) ? *ptr : NULL;
	if (!tmp) {
		return NULL;
	}
	while (tmp->next) {
		tmp = tmp->next;
	}
	if (tmp->prev) {
		tmp->prev->next = NULL;
	} else {
		*ptr = NULL;
	}
	tmp->next = NULL;
	tmp->prev = NULL;
	return tmp;
}

/* remove and return first element of list */
cache_block_t *
list_pop(cache_block_t **ptr)
{
	cache_block_t *tmp = (ptr) ? *ptr : NULL;
	if (!tmp) {
		return NULL;
	}
	if (tmp->next) {
		tmp->next->prev = NULL;
	}
	*ptr = tmp->next;
	tmp->next = NULL;
	tmp->prev = NULL;
	return tmp;
}

void
list_split(cache_block_t *ptr, cache_block_t **chunks, int n)
{
	if (!ptr) {
		return;
	}
	int len = list_length(ptr), k = len / n, i = 0, j = 0;
	while (j < n) {
		i = 0;
		chunks[j] = ptr;
		if (ptr) {
			ptr->prev = NULL;
		}
		while (ptr != NULL && ((++i < k) || (j == n - 1))) {
			ptr = ptr->next;
		}
		if (ptr) {
			ptr = ptr->next;
			if (ptr && ptr->prev) {
				ptr->prev->next = NULL;
			}
		}
		j++;
	}
}

cache_block_t *
list_get(cache_block_t **ptr, size_t n)
{
	cache_block_t *tmp = *ptr;
	size_t i = 0;
	if (!tmp) {
		return NULL;
	}
	while (tmp && i < n) {
		tmp = tmp->next;
		i = i + 1;
	}
	if (!tmp) {
		return NULL;
	}
	if (tmp->prev) {
		tmp->prev->next = tmp->next;
	} else {
		*ptr = tmp->next;
	}
	if (tmp->next) {
		tmp->next->prev = tmp->prev;
	}
	tmp->prev = NULL;
	tmp->next = NULL;
	return tmp;
}

cache_block_t *
list_slice(cache_block_t **ptr, size_t s, size_t e)
{
	cache_block_t *tmp = (ptr) ? *ptr : NULL, *ret = NULL;
	size_t i = 0;
	if (!tmp) {
		return NULL;
	}
	while (i < s && tmp) {
		tmp = tmp->next;
		i = i + 1;
	}
	if (!tmp) {
		return NULL;
	}
	// set head of new list
	ret = tmp;
	while (i < e && tmp) {
		tmp = tmp->next;
		i = i + 1;
	}
	if (!tmp) {
		return NULL;
	}
	// cut slice and return
	if (ret->prev) {
		ret->prev->next = tmp->next;
	} else {
		*ptr = tmp->next;
	}
	if (tmp->next)
		tmp->next->prev = ret->prev;
	ret->prev = NULL;
	tmp->next = NULL;
	return ret;
}

/* concat chunk of elements to the end of the list */
void
list_concat(cache_block_t **ptr, cache_block_t *chunk)
{
	cache_block_t *tmp = (ptr) ? *ptr : NULL;
	if (!tmp) {
		*ptr = chunk;
		return;
	}
	while (tmp->next != NULL) {
		tmp = tmp->next;
	}
	tmp->next = chunk;
	if (chunk) {
		chunk->prev = tmp;
	}
}

void
list_from_chunks(cache_block_t **ptr, cache_block_t **chunks, int avoid, int len)
{
	int next = (avoid + 1) % len;
	if (!(*ptr) || !chunks || !chunks[next]) {
		return;
	}
	// Disconnect avoided chunk
	cache_block_t *tmp = chunks[avoid];
	if (tmp) {
		tmp->prev = NULL;
	}
	while (tmp && tmp->next != NULL && tmp->next != chunks[next]) {
		tmp = tmp->next;
	}
	if (tmp) {
		tmp->next = NULL;
	}
	// Link rest starting from next
	tmp = *ptr = chunks[next];
	if (tmp) {
		tmp->prev = NULL;
	}
	while (next != avoid && chunks[next] != NULL) {
		next = (next + 1) % len;
		while (tmp && tmp->next != NULL && tmp->next != chunks[next]) {
			if (tmp->next) {
				tmp->next->prev = tmp;
			}
			tmp = tmp->next;
		}
		if (tmp) {
			tmp->next = chunks[next];
		}
		if (chunks[next]) {
			chunks[next]->prev = tmp;
		}
	}
	if (tmp) {
		tmp->next = NULL;
	}
}

void
print_list(cache_block_t *ptr)
{
	if (!ptr) {
		printf("(empty)\n");
		return;
	}
	while (ptr != NULL) {
		printf("%p ", (void *)ptr);
		ptr = ptr->next;
	}
	printf("\n");
}

void
initialize_list(cache_block_t *src, ul sz, ul offset)
{
	unsigned int j = 0;
	for (j = 0; j < (sz / sizeof(cache_block_t)) - offset; j++) {
		src[j].set = -2;
		src[j].delta = 0;
		src[j].prev = NULL;
		src[j].next = NULL;
	}
}

void
pick_n_random_from_list(cache_block_t *ptr, ul stride, ul sz, ul offset, ul n)
{
	unsigned int count = 1, i = 0;
	unsigned int len = ((sz - (offset * sizeof(cache_block_t))) / stride);
	cache_block_t *e = ptr;
	e->prev = NULL;
	e->set = -1;
	ul *array = (ul *)calloc(len, sizeof(ul));
	for (i = 1; i < len - 1; i++) {
		array[i] = i * (stride / sizeof(cache_block_t));
	}
	for (i = 1; i < len - 1; i++) {
		size_t j = i + rand() / (RAND_MAX / (len - i) + 1);
		int t = array[j];
		array[j] = array[i];
		array[i] = t;
	}
	for (i = 1; i < len && count < n; i++) {
		if (ptr[array[i]].set == -2) {
			e->next = &ptr[array[i]];
			ptr[array[i]].prev = e;
			ptr[array[i]].set = -1;
			e = e->next;
			count++;
		}
	}
	free(array);
	e->next = NULL;
}

void
rearrange_list(cache_block_t **ptr, ul stride, ul sz, ul offset)
{
	unsigned int len = (sz / sizeof(cache_block_t)) - offset, i = 0;
	cache_block_t *p = *ptr;
	if (!p) {
		return;
	}
	unsigned int j = 0, step = stride / sizeof(cache_block_t);
	for (i = step; i < len - 1; i += step) {
		if (p[i].set < 0) {
			p[i].set = -2;
			p[i].prev = &p[j];
			p[j].next = &p[i];
			j = i;
		}
	}
	p[0].prev = NULL;
	p[j].next = NULL;
	while (p && p->set > -1) {
		p = p->next;
	}
	*ptr = p;
	if (p) {
		p->set = -2;
		p->prev = NULL;
	}
}

void
list_set_id(cache_block_t *ptr, int id)
{
	while (ptr) {
		ptr->set = id;
		ptr = ptr->next;
	}
}

void
generate_conflict_set(cache_block_t **ptr, cache_block_t **out, int rep, int threshold)
{
	cache_block_t *candidate = NULL, *res = NULL;
	int ret = 0;
	while (*ptr) // or while size |out| == limit
	{
		candidate = list_pop(ptr);
		// ret = tests_avg(*out, (char *)candidate, conf.rounds, conf.threshold, conf.traverse);
		ret = tests_avg(*out, (char *)candidate, rep, threshold);
		if (!ret) {
			// no conflict, add element
			list_push(out, candidate);
		} else {
			// conflict, candidate goes to list of victims
			list_push(&res, candidate);
		}
	}
	*ptr = res;
}
