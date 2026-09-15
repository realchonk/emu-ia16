#include <string.h>
#include <errno.h>

static char *msg[] = {
	/* 0		*/ "Success",
	/* EPERM	*/ "Operation not permitted",
	/* ENOENT	*/ "No such file or directory",
	/* ESRCH	*/ "No such process",
	/* EINTR	*/ "Interrupted syscall",
	/* EIO		*/ "I/O error",
	/* ENODEV	*/ "No such device",
	/* E2BIG	*/ "Argument list too long",
	/* ENOEXEC	*/ "Exec format error",
	/* EBADF	*/ "Bad file descriptor",
	/* ECHILD	*/ "No child process",
	/* EAGAIN	*/ "Try again",
	/* ENOMEM	*/ "Out of memory",
	/* EACCES	*/ "Permission denied",
	/* EFAULT	*/ "Bad address",
	/* ENOTBLK	*/ "Block device required",
	/* EBUSY	*/ "Device busy",
	/* EEXIST	*/ "File exists",
	/* EXDEV	*/ "Invalid cross-device link",
	/* ENODEV	*/ "No such device",
	/* ENOTDIR	*/ "Not a directory",
	/* EISDIR	*/ "Is a directory",
	/* EINVAL	*/ "Invalid argument",
	/* ENFILE	*/ "Too many open files in system",
	/* EMFILE	*/ "Too many open files",
	/* ENOTTY	*/ "Inappropriate ioctl for device",
	/* ETXTBSY	*/ "Text file busy",
	/* EFBIG	*/ "File too large",
	/* ENOSPC	*/ "No space left on device",
	/* ESPIPE	*/ "Illegal seek",
	/* EROFS	*/ "Read-only file system",
	/* EMLINK	*/ "Too many links",
	/* EPIPE	*/ "Broken pipe",
	/* EDOM		*/ "Domain error",
	/* ERANGE	*/ "Range error",
	/* ENAMETOOLONG	*/ "File name too long",
	/* ENOSYS	*/ "Function not implemented",
};

char *
strerror (e)
int e;
{
	if (e < 0 || e > EMAXEN)
		return NULL;

	return msg[e];
}
