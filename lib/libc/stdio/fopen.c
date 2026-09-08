#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

static int
parse_mode (mode)
const char *mode;
{
	if (strcmp (mode, "r") == 0) {
		return O_RDONLY;
	} else if (strcmp (mode, "r+") == 0) {
		return O_RDWR;
	} else if (strcmp (mode, "w") == 0) {
		return O_WRONLY | O_TRUNC | O_CREAT;
	} else if (strcmp (mode, "w+") == 0) {
		return O_RDWR | O_TRUNC | O_CREAT;
	} else if (strcmp (mode, "a") == 0) {
		return O_WRONLY | O_CREAT | O_APPEND;
	} else if (strcmp (mode, "a+") == 0) {
		return O_RDWR | O_CREAT | O_APPEND;
	} else {
		errno = EINVAL;
		return -1;
	}
}

FILE *
fopen (path, mode)
const char *path, *mode;
{
	int fd, flags;

	flags = parse_mode (mode);
	if (flags == -1)
		return NULL;

	fd = open (path, flags, 0644);
	if (fd < 0)
		return NULL;

	return fdopen (fd, mode);
}
