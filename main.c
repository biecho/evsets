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
		printf("0x%016llx\n", bitmask);
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
			printf("Consistent bitmask: 0x%016llx\n", bitmask);
			consistent_count++;
		}
	}

	// Print the total number of consistent bitmasks found
	printf("Total consistent bitmasks found: %llu\n", consistent_count);
}

int
main()
{
	// Example set of addresses
	uint64_t addresses[128][16] = {
		{
			0x1471e0000,
			0x149a30000,
			0x140e00000,
			0x148320000,
			0x14e200000,
			0x1436d0000,
			0x1444d0000,
			0x146ae0000,
			0x140770000,
			0x14cd90000,
			0x14da20000,
			0x147c00000,
			0x14e140000,
			0x147a80000,
			0x14cab0000,
			0x140100000,
		},
		{
			0x14e530000,
			0x142950000,
			0x14ca50000,
			0x143ba0000,
			0x14ceb0000,
			0x142bc0000,
			0x14b5c0000,
			0x14a2f0000,
			0x1402a0000,
			0x14e070000,
			0x14c8c0000,
			0x148d60000,
			0x142800000,
			0x14f6e0000,
			0x14da40000,
			0x141310000,
		},
		{
			0x145e10000,
			0x1437e0000,
			0x1449a0000,
			0x149f90000,
			0x145930000,
			0x148000000,
			0x142780000,
			0x142310000,
			0x147d30000,
			0x149400000,
			0x1432d0000,
			0x142770000,
			0x14a3d0000,
			0x149ec0000,
			0x14b820000,
			0x149910000,
		},
		{

			0x14ccb0000,
			0x1474a0000,
			0x1432c0000,
			0x140230000,
			0x14c980000,
			0x14b470000,
			0x1447e0000,
			0x148e40000,
			0x1448e0000,
			0x141760000,
			0x14aed0000,
			0x140170000,
			0x1446b0000,
			0x145100000,
			0x14f530000,
			0x141a70000,
		},
		{
			0x14c710000,
			0x14c0c0000,
			0x149570000,
			0x140df0000,
			0x14d0a0000,
			0x1408c0000,
			0x14d3e0000,
			0x1471a0000,
			0x14fc70000,
			0x14ec10000,
			0x149110000,
			0x1464f0000,
			0x1480d0000,
			0x141ed0000,
			0x149ee0000,
			0x1455a0000,
		},
		{
			0x145ab0000,
			0x145540000,
			0x148940000,
			0x14e420000,
			0x14b7e0000,
			0x146ed0000,
			0x14ea80000,
			0x140490000,
			0x144140000,
			0x14a100000,
			0x143980000,
			0x141130000,
			0x14e630000,
			0x14d300000,
			0x144990000,
			0x143de0000,
		},
		{
			0x14e140000,
			0x142dd0000,
			0x147c00000,
			0x1427e0000,
			0x14b1c0000,
			0x144110000,
			0x14ad10000,
			0x146e70000,
			0x143920000,
			0x145dc0000,
			0x14cd60000,
			0x1457f0000,
			0x142e60000,
			0x1419b0000,
			0x145e80000,
			0x14f750000,
		},
		{

			0x14cfe0000,
			0x142fd0000,
			0x14a7b0000,
			0x14f0e0000,
			0x14a200000,
			0x144700000,
			0x1443e0000,
			0x14cb70000,
			0x142c90000,
			0x146860000,
			0x149b70000,
			0x141510000,
			0x147c60000,
			0x1457e0000,
			0x142800000,
			0x143ee0000,
		},
		{
			0x146d10000,
			0x145210000,
			0x1417c0000,
			0x14b3f0000,
			0x14e6b0000,
			0x147f60000,
			0x1446e0000,
			0x143d90000,
			0x140290000,
			0x14f2b0000,
			0x1412f0000,
			0x145000000,
			0x14f920000,
			0x14f780000,
			0x145530000,
			0x142960000,

		},
		{
			0x148950000,
			0x142c40000,
			0x149da0000,
			0x147ac0000,
			0x147b60000,
			0x1441a0000,
			0x143ec0000,
			0x140730000,
			0x145200000,
			0x141330000,
			0x145690000,
			0x1412e0000,
			0x1443c0000,
			0x148ae0000,
			0x147e20000,
			0x144290000,

		},
		{
			0x14a0c0000,
			0x149a00000,
			0x14e440000,
			0x14b230000,
			0x140480000,
			0x1417a0000,
			0x147ed0000,
			0x1447a0000,
			0x1451c0000,
			0x14a450000,
			0x1488f0000,
			0x142be0000,
			0x1413c0000,
			0x142ab0000,
			0x149f40000,
			0x14e780000,

		},
		{
			0x14da70000,
			0x142b00000,
			0x14f170000,
			0x1415a0000,
			0x143f00000,
			0x147b80000,
			0x14dd50000,
			0x14e1e0000,
			0x141d80000,
			0x143e50000,
			0x14f5e0000,
			0x147350000,
			0x148a00000,
			0x14f6a0000,
			0x1456f0000,
			0x144350000,
		},
		{
			0x143da0000,
			0x144310000,
			0x147880000,
			0x14a480000,
			0x145370000,
			0x14e3c0000,
			0x142950000,
			0x14dac0000,
			0x147e80000,
			0x14de50000,
			0x144180000,
			0x148b10000,
			0x146930000,
			0x14a060000,
			0x14b070000,
			0x140580000,
		},
		// {
		// 	0x149b10000,
		// 	0x147b50000,
		// 	0x14d270000,
		// 	0x148e40000,
		// 	0x141110000,
		// 	0x144030000,
		// 	0x14a070000,
		// 	0x142ee0000,
		// 	0x1498a0000,
		// 	0x143c90000,
		// 	0x144500000,
		// 	0x14dbf0000,
		// 	0x14e610000,
		// 	0x14f150000,
		// 	0x14a750000,
		// 	0x1454c0000,
		// },
		// {
		// 	0x14daa0000,
		// 	0x143440000,
		// 	0x149d60000,
		// 	0x14dec0000,
		// 	0x1498a0000,
		// 	0x143e80000,
		// 	0x143b40000,
		// 	0x148f10000,
		// 	0x14b140000,
		// 	0x14e130000,
		// 	0x14ff00000,
		// 	0x142cf0000,
		// 	0x14c980000,
		// 	0x14e9e0000,
		// 	0x14a330000,
		// 	0x14b5d0000,
		// },

		// {
		// 	0x14fa20000,
		// 	0x148de0000,
		// 	0x14a910000,
		// 	0x146630000,
		// 	0x149910000,
		// 	0x148210000,
		// 	0x14e9f0000,
		// 	0x147e70000,
		// 	0x141d40000,
		// 	0x14bde0000,
		// 	0x144e80000,
		// 	0x147440000,
		// 	0x14d1d0000,
		// 	0x14cd00000,
		// 	0x149ec0000,
		// 	0x146300000,
		// },
		// {
		// 	0x14e250000,
		// 	0x145540000,
		// 	0x14f4b0000,
		// 	0x148710000,
		// 	0x14e2a0000,
		// 	0x14ba00000,
		// 	0x140000000,
		// 	0x1480c0000,
		// 	0x147fe0000,
		// 	0x1441b0000,
		// 	0x148dd0000,
		// 	0x14c6a0000,
		// 	0x14dda0000,
		// 	0x142b00000,
		// 	0x144e40000,
		// 	0x1456f0000,
		// },
		// {
		// 	0x140df0000,
		// 	0x14f220000,
		// 	0x14d1f0000,
		// 	0x14efa0000,
		// 	0x14d590000,
		// 	0x148360000,
		// 	0x147490000,
		// 	0x14ec10000,
		// 	0x144980000,
		// 	0x14caf0000,
		// 	0x1419f0000,
		// 	0x14d4c0000,
		// 	0x142750000,
		// 	0x1402f0000,
		// 	0x146130000,
		// 	0x142d90000,
		// },
		// {
		// 	0x142970000,
		// 	0x149940000,
		// 	0x1413b0000,
		// 	0x14e1f0000,
		// 	0x14e440000,
		// 	0x14a630000,
		// 	0x149da0000,
		// 	0x148bb0000,
		// 	0x147ab0000,
		// 	0x14def0000,
		// 	0x1438c0000,
		// 	0x149bd0000,
		// 	0x1455d0000,
		// 	0x14b230000,
		// 	0x146ad0000,
		// 	0x14e0a0000,
		// },
		// {
		// 	0x145ff0000,
		// 	0x140070000,
		// 	0x14f1f0000,
		// 	0x14e0c0000,
		// 	0x143a40000,
		// 	0x145150000,
		// 	0x14c920000,
		// 	0x14d710000,
		// 	0x148a80000,
		// 	0x1401d0000,
		// 	0x14cfa0000,
		// 	0x14cbc0000,
		// 	0x146970000,
		// 	0x14e2d0000,
		// 	0x147f90000,
		// 	0x145ea0000,
		// },
		// {
		// 	0x14da40000,
		// 	0x142f50000,
		// 	0x146fb0000,
		// 	0x145710000,
		// 	0x149e30000,
		// 	0x1453f0000,
		// 	0x142d40000,
		// 	0x145760000,
		// 	0x148ac0000,
		// 	0x148ab0000,
		// 	0x14cc20000,
		// 	0x142c60000,
		// 	0x14e340000,
		// 	0x149fe0000,
		// 	0x142ae0000,
		// 	0x146ba0000,
		// },
		// {
		// 	0x148060000,
		// 	0x14aac0000,
		// 	0x148cd0000,
		// 	0x14dad0000,
		// 	0x1484f0000,
		// 	0x14f080000,
		// 	0x14a460000,
		// 	0x146bb0000,
		// 	0x140560000,
		// 	0x145390000,
		// 	0x1424a0000,
		// 	0x14a1a0000,
		// 	0x149b60000,
		// 	0x14f6f0000,
		// 	0x146e80000,
		// 	0x14d660000,
		// },
	};

	size_t n = sizeof(addresses) / sizeof(addresses[0]);

	// Example bit range
	uint32_t start_bit = 16;
	uint32_t end_bit = 33; // Inclusive

	// Find and print consistent bitmasks for the given range and set of addresses
	for (int i = 0; i < 12; i++) {
		find_consistent_bitmasks(&addresses[i], 16, start_bit, end_bit);
	}

	return 0;

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
	char *pool = (char *)&buffer[0];

	for (uint64_t i = 16; i < 32; i++) {
		char *victim = &buffer[(1 << 29) + i * (1 << 16)];

		cache_block_t *eviction_set = NULL;

		if (find_eviction_set(pool, pool_sz, victim, conf, &eviction_set) || !eviction_set) {
			printf("[-] Could not find all desired eviction sets.\n");
		}

		printf("[+] Found minimal eviction set for %p (length=%d): \n", (void *)victim,
		       list_length(eviction_set));

		cache_block_t *ptr = eviction_set;
		while (ptr != NULL) {
			uint64_t set_index = extract_bits(ptr, 10, 6);
			assert(set_index == 0);
			printf("%#lx\n", read_from_pagemap((void *)ptr));
			ptr = ptr->next;
		}
		printf("\n");
	}

	munmap(buffer, 1 << 30);
	return 0;
}
