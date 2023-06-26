#include <sys/mman.h>
#include <sys/uio.h>

#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <liburing.h>
#include <stdatomic.h>

#include "../shared.h"

#define QUEUE_DEPTH	1
#define BLOCKSZ		1024

/* straight-out of liburing */
#define WRITE_ONCE(var, val)						\
	atomic_store_explicit((_Atomic __typeof__(var) *)&(var),	\
			      (val), memory_order_relaxed)
#define READ_ONCE(var)							\
	atomic_load_explicit((_Atomic __typeof__(var) *)&(var),		\
			     memory_order_relaxed)

#define smp_store_release(p, v)						\
	atomic_store_explicit((_Atomic __typeof__(*(p)) *)(p), (v), \
			      memory_order_release)
#define smp_load_acquire(p)						\
	atomic_load_explicit((_Atomic __typeof__(*(p)) *)(p),	\
			     memory_order_acquire)

#define smp_mb()							\
	atomic_thread_fence(memory_order_seq_cst)

/*
 * SQ
 *     ---  elm2   ---
 *   / /  /      \  \  \
 *  / /  /        \  \  \
 * | elm1|         | elm3| <--- tail (userspace)
 *  \  \  \       /     /
 *   \  \  \     /     /
 *     ---  elm0  ---
 *           ^
 *           |
 *           |
 *        head (kernel)
 *
 * array:
 *  ---- ---- ---- ----
 * |elm0|elm1|elm2|elm3|
 *  ---- ---- ---- ----
 *    0    1    2    3
 *
 * ring_entries = 4
 * ring_mask = 0x3
 */
struct app_io_sq_ring {
	uint32_t	*head;
	uint32_t	*tail;
	uint32_t	*ring_mask;
	uint32_t	*ring_entries;
	uint32_t	*flags;
	uint32_t	*array;
};

struct app_io_cq_ring {
	uint32_t	*head;
	uint32_t	*tail;
	uint32_t	*ring_mask;
	uint32_t	*ring_entries;
	struct	io_uring_cqe *cqes;
};

struct submitter {
	int	ring_fd;
	struct	app_io_sq_ring	 sq_ring;
	struct	io_uring_sqe	*sqes;
	struct	app_io_cq_ring	 cq_ring;
};

/* used by readv/writev */
struct file_info {
	int	fd;
	size_t	fsize;
	size_t	nblocks;
	struct	iovec	*iov;
};

static void
app_setup_uring(struct submitter *s)
{
	uint32_t sq_len, cq_len;
	void *sq_ptr, *cq_ptr;
	struct io_uring_params p;
	struct app_io_cq_ring *cring = &s->cq_ring;
	struct app_io_sq_ring *sring = &s->sq_ring;

	bzero(&p, sizeof(p));
	s->ring_fd = io_uring_setup(QUEUE_DEPTH, &p);
	if (s->ring_fd < 0)
		err(1, "io_uring_setup");

	sq_len = p.sq_off.array + p.sq_entries * sizeof(unsigned);
	/* this is incorrect if flag IORING_SETUP_CQE32 is used */
	cq_len = p.cq_off.cqes + p.cq_entries * sizeof(struct io_uring_cqe);

	if (p.features & IORING_FEAT_SINGLE_MMAP) {
		if (cq_len > sq_len)
			sq_len = cq_len;
		cq_len = sq_len;
	}

	sq_ptr = mmap(0, sq_len, PROT_READ | PROT_WRITE,
		      MAP_SHARED | MAP_POPULATE, s->ring_fd, IORING_OFF_SQ_RING);
	if (sq_ptr == MAP_FAILED)
		err(1, "mmap");

	if (p.features & IORING_FEAT_SINGLE_MMAP)
		cq_ptr = sq_ptr;
	else {
		cq_ptr = mmap(0, cq_len, PROT_READ | PROT_WRITE,
			      MAP_SHARED | MAP_POPULATE, s->ring_fd,
			      IORING_OFF_CQ_RING);
	}

	sring->head = sq_ptr + p.sq_off.head;
	sring->tail = sq_ptr + p.sq_off.tail;
	sring->ring_mask = sq_ptr + p.sq_off.ring_mask;
	sring->ring_entries = sq_ptr + p.sq_off.ring_entries;
	sring->flags = sq_ptr + p.sq_off.flags;
	sring->array = sq_ptr + p.sq_off.array;

	/* wrong value if IORING_SETUP_SQE128 */
	s->sqes = mmap(0, p.sq_entries * sizeof(struct io_uring_sqe),
		       PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
		       s->ring_fd, IORING_OFF_SQES);
	if (s->sqes == NULL)
		err(1, "mmap");

	cring->head = cq_ptr + p.cq_off.head;
	cring->tail = cq_ptr + p.cq_off.tail;
	cring->ring_mask = cq_ptr + p.cq_off.ring_mask;
	cring->ring_entries = cq_ptr + p.cq_off.ring_entries;
	cring->cqes = cq_ptr + p.cq_off.cqes;
}

static void
add_to_sq_tail(struct submitter *s, struct file_info *fi)
{
	uint32_t index;
	uint32_t tail;
	struct io_uring_sqe *sqe;
	struct app_io_sq_ring *sring = &s->sq_ring;

	tail = smp_load_acquire(sring->tail);

	index = tail & *s->sq_ring.ring_mask;
	sqe = &s->sqes[index];

	sqe->fd = fi->fd;
	sqe->flags = 0;
	sqe->opcode = IORING_OP_READV;
	sqe->addr = (__u64)fi->iov;
	sqe->len = (uint32_t)fi->nblocks;
	sqe->off = 0;
	sqe->user_data = (__u64)fi;
	sring->array[index] = index;

	smp_store_release(sring->tail, tail + 1);
}

static void
read_from_cq(struct submitter *s, const struct file_info *fi_src)
{
	struct app_io_cq_ring *cring = &s->cq_ring;

	while (1) {
		uint32_t head;
		size_t i;
		struct file_info *fi;
		struct io_uring_cqe *cqe;

		head = smp_load_acquire(cring->head);

		/* empty ring */
		if (head == *cring->tail)
			break;

		cqe = &cring->cqes[head & *s->cq_ring.ring_mask];
		fi = (struct file_info *)cqe->user_data;
		if (cqe->res < 0) {
			errno = -cqe->res;
			err(1, "cq ring");
		}
		if (memcmp(fi_src, fi, sizeof(*fi)) != 0)
			printf("file_info structure does not match\n");

		for (i = 0; i < fi->nblocks; i++) {
			output_to_console(fi->iov[i].iov_base,
					  fi->iov[i].iov_len);
			free(fi->iov[i].iov_base);
		}

		smp_store_release(cring->head, head + 1);
		free(fi->iov);
	}
}

static void
submit_to_sq(const char *fname, struct submitter *s, struct file_info *fi)
{
	int ret;

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

	add_to_sq_tail(s, fi);

	ret = io_uring_enter(s->ring_fd, 1, 1, IORING_ENTER_GETEVENTS, NULL);
	if (ret < 0)
		err(1, "io_uring_enter");
}

int
main(int argc, char **argv)
{
	int i;
	struct submitter *s;
	struct file_info fi;

	if (argc < 2)
		err(1, "Usage: %s <filename> ...", argv[0]);

	s = calloc(1, sizeof(*s));
	if (s == NULL)
		err(1, "calloc");

	app_setup_uring(s);

	for (i = 1; i < argc; i++) {
		submit_to_sq(argv[i], s, &fi);
		read_from_cq(s, &fi);
	}
	free(s);
	return 0;
}
