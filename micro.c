#include "micro.h"
#include "cache.h"

#include <fcntl.h>
#include <unistd.h>

ul
vtop(ul vaddr)
{
	int fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	unsigned long paddr = -1;
	unsigned long index = (vaddr / PAGE_SIZE2) * sizeof(paddr);
	if (pread(fd, &paddr, sizeof(paddr), index) != sizeof(paddr)) {
		return -1;
	}
	close(fd);
	paddr &= 0x7fffffffffffff;
	return (paddr << PAGE_BITS) | (vaddr & (PAGE_SIZE2 - 1));
}

unsigned int
count_bits(ul n)
{
	unsigned int count = 0;
	while (n) {
		n &= (n - 1);
		count++;
	}
	return count;
}

unsigned int
nbits(ul n)
{
	unsigned int ret = 0;
	n = n >> 1;
	while (n > 0) {
		n >>= 1;
		ret++;
	}
	return ret;
}

ul
ptos(ul paddr, ul slices)
{
	unsigned long long ret = 0;
	unsigned long long mask[3] = { 0x1b5f575440ULL, 0x2eb5faa880ULL,
				       0x3cccc93100ULL }; // according to Maurice et al.
	int bits = nbits(slices) - 1;
	switch (bits) {
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

