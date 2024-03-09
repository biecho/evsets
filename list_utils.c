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

/**
 * Extracts the first n bits of a value, offset by k bits.
 * 
 * @param value The value from which bits are to be extracted.
 * @param n The number of bits to extract.
 * @param k The offset from which to start bit extraction.
 * @return The extracted bits as the least significant bits of the result.
 */
uint64_t extract_bits(uint64_t value, unsigned int n, unsigned int k) {
    // Ensure n and k are within reasonable bounds.
    if (n > 64) n = 64;
    if (k > 64) k = 64;
    
    // Right shift by k to discard the first k bits.
    // Then apply a bitmask to retain only the first n bits.
    uint64_t mask = (1ULL << n) - 1;
    return (value >> k) & mask;
}


void
print_list(cache_block_t *ptr)
{
	if (!ptr) {
		printf("(empty)\n");
		return;
	}
	while (ptr != NULL) {
		uint64_t set_index = extract_bits(ptr, 11, 6);
		printf("Set index: 0x%llx ", set_index);
		printf("%p\n", (void *)ptr);
		ptr = ptr->next;
	}
	printf("\n");
}

void
initialize_list(cache_block_t *src, unsigned long sz)
{
	unsigned int j = 0;
	for (j = 0; j < (sz / sizeof(cache_block_t)); j++) {
		src[j].set = -2;
		src[j].delta = 0;
		src[j].prev = NULL;
		src[j].next = NULL;
	}
}

/**
 * Randomly selects n elements from a set of cache blocks and re-links them into a new list.
 * This function assumes the set is initially laid out in an array with a specified stride between elements.
 * 
 * @param set Pointer to the array of cache blocks.
 * @param stride Distance (in bytes) between consecutive cache blocks in the array.
 * @param set_size Total size (in bytes) of the array containing the cache blocks.
 * @param n Number of blocks to randomly select and link.
 */
void
pick_n_random_from_list(cache_block_t *set, unsigned long stride, unsigned long set_size, unsigned long n)
{
	unsigned int num_blocks = set_size / stride; // Calculate number of blocks in the set.
	cache_block_t *current_block = set;
	current_block->prev = NULL; // Initialize the first block.
	current_block->set = -1;

	// Allocate an array to hold block indices for random selection.
	unsigned long *indices = (unsigned long *)calloc(num_blocks, sizeof(unsigned long));
	for (unsigned int i = 0; i < num_blocks; i++) {
		indices[i] = i * (stride / sizeof(cache_block_t)); // Calculate proper index based on stride.
	}

	// Shuffle indices using the Fisher-Yates algorithm.
	for (unsigned int i = 0; i < num_blocks - 1; i++) {
		size_t rand_index = i + rand() / (RAND_MAX / (num_blocks - i) + 1);
		unsigned long temp = indices[rand_index];
		indices[rand_index] = indices[i];
		indices[i] = temp;
	}

	// Link n randomly selected blocks.
	unsigned int selected_count = 1; // Start with 1 to account for the initial block.
	for (unsigned int i = 0; i < num_blocks && selected_count < n; i++) {
		if (set[indices[i]].set == -2) { // Check if the block is eligible for selection.
			current_block->next = &set[indices[i]]; // Link the block.
			set[indices[i]].prev = current_block;
			set[indices[i]].set = -1;
			current_block = current_block->next;
			selected_count++;
		}
	}

	free(indices); // Free the allocated indices array.
	current_block->next = NULL; // Mark the end of the list.
}
