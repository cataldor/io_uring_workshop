#ifndef __SHARED_H__
#define __SHARED_H__

#include <sys/stat.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static inline size_t
nb_blocks(size_t fsize, size_t blocksz)
{
	size_t ret;

	ret = fsize / blocksz;
	if (fsize % blocksz)
		ret++;

	return ret;
}

static inline int
file_size(int fd, size_t *fsize)
{
	int ret;
	struct stat st;

	ret = fstat(fd, &st);
	if (ret < 0)
		return -1;
	*fsize = st.st_size;
	return 0;
}

static inline int
fill_iovec(struct iovec *iov, size_t iov_len, size_t fsize, size_t nblocks,
	   size_t blocksz)
{
	int ret;
	size_t i = 0;

	while (fsize) {
		void *buf = NULL;
		const size_t bytes = fsize > blocksz ? blocksz : fsize;

		ret = posix_memalign(&buf, blocksz, blocksz);
		if (ret < 0)
			return -1;

		if (i >= iov_len)
			return -1;

		iov[i].iov_base = buf;
		iov[i].iov_len = bytes;
		i++;
		fsize -= bytes;
	}
	return 0;
}

static inline void
output_to_console(char *buf, size_t buf_len)
{
	while (buf_len--)
		fputc(*buf++, stdout);
}

#endif
