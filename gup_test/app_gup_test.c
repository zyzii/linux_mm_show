#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#define PAGE_NUM	4

#define PM_PFRAME_BITS		55
#define PM_PFRAME_MASK		((1LL << PM_PFRAME_BITS) - 1)

int main()
{
	int fd, pfn_fd;
	void *buf;
	int ret, i;
	char *p;
	int pagesize = getpagesize();
	unsigned long offset;
	char pfn_buf[100] = {};
	unsigned long *pfns = (unsigned long *)pfn_buf;

	/* Open files */
	fd = open("/dev/guptest", O_RDWR);
	pfn_fd = open("/proc/self/pagemap", O_RDONLY);
	if (fd < 0 || pfn_fd < 0) {
		printf("error in open files\n");
		return -1;
	}

	/*
	 * Allocate four pages and populate the page 1 and page 3
	 *
	 *    -------------------------------------------------
	 *    |          | page 1    |           | page 3      |
	 *    -------------------------------------------------
	 *
	 *    page 0 and page 2 are not populated.
	 */
	ret = posix_memalign(&buf, pagesize, pagesize * PAGE_NUM);
	if (ret)
		goto err;
	printf("pagesize : %d, buf: %p\n\n", pagesize, buf);

	p = buf;
	p[pagesize] = 1;	/* raise a page fault */
	p[pagesize * 3] = 1;	/* raise a page fault */

	/* check the PFNs now */
	offset = (unsigned long)buf;
	offset = offset / pagesize;
	offset *= 8;

	pread(pfn_fd, pfn_buf, 8 * PAGE_NUM, offset);/* 8 bytes per PFN */
	for (i = 0; i < PAGE_NUM; i++) {
		if (pfns[i] & PM_PFRAME_MASK)
			printf("\tpage %d: %#lx, pfn: %#llx\n", i, pfns[i], pfns[i] & PM_PFRAME_MASK);
		else
			printf("\tpage %d: < empty >\n", i);
	}

	/* Finially, we do the read now, it is DMA read in the driver. */
	read(fd, buf, pagesize * PAGE_NUM);

	free(buf);
err:
	close(pfn_fd);
	close(fd);
	return 0;
}
