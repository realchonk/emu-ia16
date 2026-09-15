#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>

#define LEN(a) (sizeof (a) / sizeof (*(a)))

extern char *text, *emu;

static int
xoflags (int oflags)
{
	int nflags = 0;

	switch (oflags & 3) {
	case 0:
		nflags = O_RDONLY;
		break;
	case 1:
		nflags = O_WRONLY;
		break;
	case 2:
		nflags = O_RDWR;
		break;
	default:
		return 0;
	}

	if (oflags & 0x0008)
		nflags |= O_APPEND;
	if (oflags & 0x0200)
		nflags |= O_CREAT;
	if (oflags & 0x0400)
		nflags |= O_TRUNC;
	if (oflags & 0x0800)
		nflags |= O_EXCL;

	return nflags;
}

static int
doexecv (char *prog, uint16_t *argv)
{
	char	**hargv;
	size_t	  i, argc;
	int	  ret;

	for (argc = 0; argv[argc] != 0; ++argc);

	hargv = calloc (argc + 2, sizeof (char *));
	hargv[0] = emu;
	for (i = 0; i < argc; ++i) {
		hargv[i + 1] = text + argv[i];
	}
	hargv[argc + 1] = NULL;
	
	ret = execv (emu, hargv);
	warn ("execv");
	free (hargv);
	return ret;
}

static int
mapws (int ws)
{
	if (WIFEXITED (ws)) {
		return WEXITSTATUS (ws) << 8;
	} else {
		/* TODO */
		return -1;
	}
}

/* must match lib/libc/include/errno.h */
static int errtab[] = {
	[EPERM]		= 1,
	[ENOENT]	= 2,
	[ESRCH]		= 3,
	[EINTR]		= 4,
	[EIO]		= 5,
	[ENXIO]		= 6,
	[E2BIG]		= 7,
	[ENOEXEC]	= 8,
	[EBADF]		= 9,
	[ECHILD]	= 10,
	[EAGAIN]	= 11,
	[ENOMEM]	= 12,
	[EACCES]	= 13,
	[EFAULT]	= 14,
	[ENOTBLK]	= 15,
	[EBUSY]		= 16,
	[EEXIST]	= 17,
	[EXDEV]		= 18,
	[ENODEV]	= 19,
	[ENOTDIR]	= 20,
	[EISDIR]	= 21,
	[EINVAL]	= 22,
	[ENFILE]	= 23,
	[EMFILE]	= 24,
	[ENOTTY]	= 25,
	[ETXTBSY]	= 26,
	[EFBIG]		= 27,
	[ENOSPC]	= 28,
	[ESPIPE]	= 29,
	[EROFS]		= 30,
	[EMLINK]	= 31,
	[EPIPE]		= 32,
	[EDOM]		= 33,
	[ERANGE]	= 34,
	[ENAMETOOLONG]	= 35,
	[ENOSYS]	= 36,
};

static int
map (int ec)
{
	if (ec != -1)
		return ec;

	return errno < LEN (errtab) && errtab[errno] != 0 ? -errtab[errno] : -1;
}

int
sysentry(int ss, uint32_t esp, int no)
{
	char		*linsp;
	uint16_t	*args;
	int		 r, ws;

	linsp = text + (esp & 0xffff);
	args = (uint16_t *)(linsp + 6);

	switch (no) {
	case 0:
		return sysentry (ss, esp + 2, args[0]);
	case 1:
		_exit(args[0]);
	case 2:
		return map (write (args[0], text + args[1], args[2]));
	case 3:
		return map (read (args[0], text + args[1], args[2]));
	case 4:
		return map (close (args[0]));
	case 5:
		return map (lseek (args[0], args[1] | (args[2] << 16), args[3]));
	case 6:
		return map (unlink (text + args[0]));
	case 7:
		return map (open (text + args[0], xoflags (args[1]), args[2]));
	case 8:
		return map (creat (text + args[0], args[1]));
	case 9:
		return map (fork ());
	case 10:
		return map (doexecv (text + args[0], (uint16_t *)(text + args[1])));
	case 11:
		r = wait (&ws);
		if (args[0] != 0)
			*((uint16_t *)(text + args[0])) = mapws (ws);
		return map (r);

	default:
		return -ENOSYS;
	}
}
