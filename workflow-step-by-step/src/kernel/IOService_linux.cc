#include <sys/syscall.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "list.h"
#include "IOService_linux.h"

/* Linux async I/O interface from libaio.h */

