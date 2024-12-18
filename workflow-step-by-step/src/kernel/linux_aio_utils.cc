#include <sys/syscall.h>
#include <unistd.h>

typedef struct io_context *io_context_t;

typedef enum io_iocb_cmd {
	IO_CMD_PREAD = 0,
	IO_CMD_PWRITE = 1,

	IO_CMD_FSYNC = 2,
	IO_CMD_FDSYNC = 3,

	IO_CMD_NOOP = 6,
	IO_CMD_PREADV = 7,
	IO_CMD_PWRITEV = 8,
} io_iocb_cmd_t;

#if defined(__i386__) /* little endian, 32 bits */
#define PADDED(x, y)	x; unsigned y
#define PADDEDptr(x, y)	x; unsigned y
#define PADDEDul(x, y)	unsigned long x; unsigned y
#elif defined(__ia64__) || defined(__x86_64__) || defined(__alpha__)
#define PADDED(x, y)	x, y
#define PADDEDptr(x, y)	x
#define PADDEDul(x, y)	unsigned long x
#elif defined(__powerpc64__) /* big endian, 64 bits */
#define PADDED(x, y)	unsigned y; x
#define PADDEDptr(x,y)	x
#define PADDEDul(x, y)	unsigned long x
#elif defined(__PPC__)  /* big endian, 32 bits */
#define PADDED(x, y)	unsigned y; x
#define PADDEDptr(x, y)	unsigned y; x
#define PADDEDul(x, y)	unsigned y; unsigned long x
#elif defined(__s390x__) /* big endian, 64 bits */
#define PADDED(x, y)	unsigned y; x
#define PADDEDptr(x,y)	x
#define PADDEDul(x, y)	unsigned long x
#elif defined(__s390__) /* big endian, 32 bits */
#define PADDED(x, y)	unsigned y; x
#define PADDEDptr(x, y) unsigned y; x
#define PADDEDul(x, y)	unsigned y; unsigned long x
#elif defined(__arm__)
#  if defined (__ARMEB__) /* big endian, 32 bits */
#define PADDED(x, y)	unsigned y; x
#define PADDEDptr(x, y)	unsigned y; x
#define PADDEDul(x, y)	unsigned y; unsigned long x
#  else                   /* little endian, 32 bits */
#define PADDED(x, y)	x; unsigned y
#define PADDEDptr(x, y)	x; unsigned y
#define PADDEDul(x, y)	unsigned long x; unsigned y
#  endif
#elif defined(__aarch64__)
#  if defined (__AARCH64EB__) /* big endian, 64 bits */
#define PADDED(x, y)    unsigned y; x
#define PADDEDptr(x,y)  x
#define PADDEDul(x, y)  unsigned long x
#  elif defined(__AARCH64EL__) /* little endian, 64 bits */
#define PADDED(x, y)    x, y
#define PADDEDptr(x, y) x
#define PADDEDul(x, y)  unsigned long x
#  endif
#else
#error	endian?
#endif

struct io_iocb_common {
	PADDEDptr(void *buf, __pad1);
	PADDEDul(nbytes, __pad2);
	long long offset;
	long long __pad3;
	unsigned flags;
	unsigned resfd;
};	/* result code is the amount read or -'ve errno */

struct io_iocb_vector {
	const struct iovec *vec;
	int nr;
	long long offset;
};	/* result code is the amount read or -'ve errno */

struct iocb {
	PADDEDptr(void *data, __pad1);	/* Return in the io completion event */
	PADDED(unsigned key, __pad2);	/* For use in identifying io requests */

	short aio_lio_opcode;	
	short aio_reqprio;
	int aio_fildes;

	union {
		struct io_iocb_common c;
		struct io_iocb_vector v;
	} u;
};

struct io_event {
	PADDEDptr(void *data, __pad1);
	PADDEDptr(struct iocb *obj, __pad2);
	PADDEDul(res, __pad3);
	PADDEDul(res2, __pad4);
};

#undef PADDED
#undef PADDEDptr
#undef PADDEDul

/* Actual syscalls */
static inline int io_setup(int maxevents, io_context_t *ctxp)
{
	return syscall(__NR_io_setup, maxevents, ctxp);
}

static inline int io_destroy(io_context_t ctx)
{
	return syscall(__NR_io_destroy, ctx);
}

static inline int io_submit(io_context_t ctx, long nr, struct iocb *ios[])
{
	return syscall(__NR_io_submit, ctx, nr, ios);
}

static inline int io_cancel(io_context_t ctx, struct iocb *iocb,
							struct io_event *evt)
{
	return syscall(__NR_io_cancel, ctx, iocb, evt);
}

static inline int io_getevents(io_context_t ctx_id, long min_nr, long nr,
							   struct io_event *events,
							   struct timespec *timeout)
{
	return syscall(__NR_io_getevents, ctx_id, min_nr, nr, events, timeout);
}

static inline void io_set_eventfd(struct iocb *iocb, int eventfd)
{
	iocb->u.c.flags |= (1 << 0) /* IOCB_FLAG_RESFD */;
	iocb->u.c.resfd = eventfd;
}