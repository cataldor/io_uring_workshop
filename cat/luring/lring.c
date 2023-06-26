#include <sys/mman.h>
#include <sys/uio.h>

#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <liburing.h>

#include "../shared.h"

#define QUEUE_DEPTH	1
#define BLOCKSZ		1024

/* used by readv/writev */
struct file_info {
	int	fd;
	size_t	fsize;
	size_t	nblocks;
	struct	iovec	*iov;
};

static void
read_from_cq(struct file_info *fi, struct io_uring *ring)
{
	int ret;
	size_t i;
	struct io_uring_cqe *cqe;
	struct file_info *fi_cqe;

	ret = io_uring_wait_cqe(ring, &cqe);
	if (ret < 0)
		err(1, "io_uring_wait_cqe");
	if (cqe->res < 0) {
		errno = -cqe->res;
		err(1, "io_uring readv failed");
	}
	fi_cqe = io_uring_cqe_get_data(cqe);

	if (memcmp(fi_cqe, fi, sizeof(*fi)) != 0)
		printf("file_info structure does not match\n");

	for (i = 0; i < fi->nblocks; i++) {
		output_to_console(fi->iov[i].iov_base, fi->iov[i].iov_len);
		free(fi->iov[i].iov_base);
	}

	io_uring_cqe_seen(ring, cqe);
	free(fi->iov);
}

static void
submit_to_sq(const char *fname, struct file_info *fi, struct io_uring *ring)
{
	int ret;
	struct io_uring_sqe *sqe;

	fi->fd = open(fname, O_RDONLY);
	if (fi->fd < 0)
		err(1, "open");

	ret = file_size(fi->fd, &fi->fsize);
	if (ret < 0)
		err(1, "file_size");

	fi->nblocks = nb_blocks(fi->fsize, BLOCKSZ);
	fi->iov = calloc(fi->nblocks, sizeof(struct iovec));
	if (fi->iov == NULL)
		err(1, "calloc");

	ret = fill_iovec(fi->iov, fi->nblocks, fi->fsize, fi->nblocks, BLOCKSZ);
	if (ret < 0)
		err(1, "fill_iovec");

	sqe = io_uring_get_sqe(ring);
	if (sqe == NULL)
		err(1, "full queue when it should not");

	io_uring_prep_readv(sqe, fi->fd, fi->iov, (int)fi->nblocks, 0);
	io_uring_sqe_set_data(sqe, fi);
	io_uring_submit(ring);
}

int
main(int argc, char **argv)
{
	int ret;
	int i;
	struct file_info fi;
	struct io_uring ring;

	if (argc < 2)
		err(1, "Usage: %s <filename> ...", argv[0]);

	ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
	if (ret < 0) {
		errno = -ret;
		err(1, "io_uring_queue_init");
	}

	for (i = 1; i < argc; i++) {
		submit_to_sq(argv[i], &fi, &ring);
		read_from_cq(&fi, &ring);
	}
	return 0;
}

