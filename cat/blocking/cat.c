#include <sys/uio.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../shared.h"

#define BLOCKSZ	4096

static int
read_and_print(const char *fname)
{
	int ret;
	int fd;
	size_t fsize;
	size_t i;
	size_t nblocks;
	ssize_t ssret;
	struct iovec *iov;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = file_size(fd, &fsize);
	if (ret < 0)
		return -1;

	nblocks = nb_blocks(fsize, BLOCKSZ);
	iov = calloc(nblocks, sizeof(*iov));
	if (iov == NULL)
		return -1;

	ret = fill_iovec(iov, nblocks, fsize, nblocks, BLOCKSZ);
	if (ret < 0)
		return -1;

	ssret = readv(fd, iov, (int)nblocks);
	if (ssret < 0)
		return -1;
	if (ssret != (ssize_t)fsize)
		printf("expected %zu got %zd\n", fsize, ssret);

	for (i = 0; i < nblocks; i++) {
		output_to_console(iov[i].iov_base, iov[i].iov_len);
		free(iov[i].iov_base);
	}

	free(iov);
	return 0;
}

int
main(int argc, char **argv)
{
	int i;

	if (argc < 2)
		err(1, "%s <file> ...", argv[0]);

	for (i = 1; i < argc; i++) {
		const int ret = read_and_print(argv[i]);
		if (ret < 0)
			err(1, "read_and_print");
	}
	return 0;
}
