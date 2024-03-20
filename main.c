#include "cache.h"
#include "eviction.h"
#include "list_utils.h"

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NUM_MASKS 64

uint64_t
read_from_pagemap(void *virutal_address)
{
	int pagemap_fd;
	uint64_t paddr = 0;
	off_t offset;
	ssize_t bytes_read;
	const size_t pagemap_entry_size = sizeof(uint64_t);
	uint64_t vaddr = (uint64_t)virutal_address;
	unsigned long page_offset = vaddr % sysconf(_SC_PAGESIZE); // Calculate the offset within the page

	pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
	if (pagemap_fd < 0) {
		perror("open pagemap");
		return 0;
	}

	// Calculate the offset for the virtual address in the pagemap file
	offset = (vaddr / sysconf(_SC_PAGESIZE)) * pagemap_entry_size;

	if (lseek(pagemap_fd, offset, SEEK_SET) == (off_t)-1) {
		perror("lseek pagemap");
		close(pagemap_fd);
		return 0;
	}

	bytes_read = read(pagemap_fd, &paddr, pagemap_entry_size);
	if (bytes_read < 0) {
		perror("read pagemap");
		close(pagemap_fd);
		return 0;
	}

	close(pagemap_fd);

	// Extract the physical page frame number and calculate the physical address
	paddr = paddr & ((1ULL << 55) - 1); // Mask out the flag bits
	paddr = paddr * sysconf(_SC_PAGESIZE); // Convert page frame number to physical address
	paddr += page_offset; // Add the offset within the page

	return paddr;
}

// Function to XOR selected bits of an address based on a bitmask
uint8_t
xor_selected_bits(uint64_t address, uint64_t bitmask)
{
	uint8_t result = 0;

	for (uint64_t bit = 1; bit != 0; bit <<= 1) {
		if (bitmask & bit) {
			result ^= (address & bit) ? 1 : 0;
		}
	}

	return result;
}

uint64_t
compute_xor_mask_result(uint64_t address, uint64_t masks[], int n)
{
	uint64_t result_mask = 0;

	for (int i = 0; i < n; ++i) {
		// Apply xor_selected_bits for each mask and set the bit in result_mask accordingly
		if (xor_selected_bits(address, masks[i])) {
			result_mask |= (uint64_t)1 << i;
		}
	}

	return result_mask;
}

// Function to check if XOR of selected bits is consistent across a set of addresses
bool
check_xor_consistency(uint64_t addresses[], size_t n, uint64_t bitmask)
{
	if (n == 0) {
		return true;
	}

	// Get initial XOR result for comparison
	uint8_t initial_result = xor_selected_bits(addresses[0], bitmask);

	for (size_t i = 1; i < n; ++i) {
		uint8_t current_result = xor_selected_bits(addresses[i], bitmask);

		// If any address yields a different result, the XOR is not consistent
		if (current_result != initial_result) {
			return false;
		}
	}

	return true;
}

// Function to generate and print all possible bitmasks within a specified bit range
void
generate_bitmasks(uint32_t start_bit, uint32_t end_bit)
{
	if (start_bit > end_bit || end_bit >= 64) {
		printf("Invalid bit range.\n");
		return;
	}

	uint32_t num_bits = end_bit - start_bit + 1;
	uint64_t total_combinations = (uint64_t)1 << num_bits; // 2^num_bits

	printf("Generating all possible bitmasks for bits %u to %u (inclusive):\n", start_bit, end_bit);

	for (uint64_t i = 0; i < total_combinations; i++) {
		uint64_t bitmask = i << start_bit; // Shift to correct position within the 64-bit address
		printf("0x%016lx\n", bitmask);
	}
}

// Function to identify and print all bitmasks that are consistent across a set of addresses
void
find_consistent_bitmasks(uint64_t addresses[], size_t n, uint32_t start_bit, uint32_t end_bit)
{
	if (start_bit > end_bit || end_bit >= 64) {
		printf("Invalid bit range.\n");
		return;
	}
	uint32_t num_bits = end_bit - start_bit + 1;
	uint64_t total_combinations = (uint64_t)1 << num_bits; // 2^num_bits

	// Counter for the number of consistent bitmasks found
	uint64_t consistent_count = 0;

	for (uint64_t i = 0; i < total_combinations; i++) {
		uint64_t bitmask = i << start_bit; // Shift to correct position
		if (check_xor_consistency(addresses, n, bitmask)) {
			printf("Consistent bitmask: 0x%016lx\n", bitmask);
			consistent_count++;
		}
	}

	// Print the total number of consistent bitmasks found
	printf("Total consistent bitmasks found: %lu\n", consistent_count);
}

unsigned int
count_bits(uint64_t n)
{
	unsigned int count = 0;
	while (n)
	{
		n &= (n-1) ;
		count++;
	}
	return count;
}

unsigned int
nbits(uint64_t n)
{
	unsigned int ret = 0;
	n = n >> 1;
	while (n > 0)
	{
		n >>= 1;
		ret++;
	}
	return ret;
}

uint64_t
ptos(uint64_t paddr, uint64_t slices)
{
	unsigned long long ret = 0;
	unsigned long long mask[3] = {0x1b5f575440ULL, 0x2eb5faa880ULL, 0x3cccc93100ULL}; // according to Maurice et al.
	int bits = nbits(slices) - 1;
	switch (bits)
	{
		case 3:
			ret = (ret << 1) | (unsigned long long)(count_bits(mask[2] & paddr) % 2);
		case 2:
			ret = (ret << 1) | (unsigned long long)(count_bits(mask[1] & paddr) % 2);
		case 1:
			ret = (ret << 1) | (unsigned long long)(count_bits(mask[0] & paddr) % 2);
		default:
		break;
	}
	return ret;
}

int
main()
{
	int seed = time(NULL);
	srand(seed);

	struct eviction_config_t conf = {
		.rounds = 10,
		.cal_rounds = 1000000,
		.stride = 4096,
		.cache_size = 12 << 20,
		.cache_way = 16,
		.cache_slices = 6,
		.initial_set_size = 8192,
	};

	char *buffer = (char *)mmap(NULL, 1 << 30, PROT_READ | PROT_WRITE,
				    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
	if (buffer == MAP_FAILED) {
		printf("[!] Error: Memory allocation failed\n");
		return 1;
	}

	// Consider the first 128MB as pool
	unsigned long long pool_sz = 256 << 20;
	char *pool = (char *)&buffer[1 << 29];

	for (uint64_t i = 0; i < 1; i++) {
		char *victim = &buffer[i * (1 << 16)];

		cache_block_t *eviction_set = NULL;

		if (find_eviction_set(pool, pool_sz, victim, conf, &eviction_set) || !eviction_set) {
			printf("[-] Could not find all desired eviction sets.\n");
		}

		printf("[+] Found minimal eviction set for %p (length=%d): \n", (void *)victim,
		       list_length(eviction_set));

		cache_block_t *ptr = eviction_set;
		while (ptr != NULL) {
			uint64_t set_index = extract_bits((uint64_t)ptr, 11, 6);
			// assert(set_index == 0);

			uint64_t paddr = read_from_pagemap((void*)ptr);
			uint64_t slice = ptos(paddr, 6);

			printf("%#lx (%lu/%lu)\n", paddr, slice, set_index);

			ptr = ptr->next;
		}
		printf("\n");
	}

	munmap(buffer, 1 << 30);
	return 0;
}
