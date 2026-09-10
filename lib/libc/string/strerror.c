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
};

char *
strerror (e)
int e;
{
	if (e < 0 || e > EMAXEN)
		return NULL;

	return msg[e];
}
