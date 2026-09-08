#include <stdio.h>
#include <errno.h>

size_t
fwrite (ptr, size, n, file)
const void	*ptr;
size_t		 size, n;
FILE		*file;
{
	if (file == NULL || file->writefn == NULL) {
		errno = EINVAL;
		return EOF;
	}
	return file->writefn (file->cookie, ptr, size * n);
}
