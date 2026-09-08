#include <stdio.h>
#include <errno.h>

size_t
fread (ptr, size, n, file)
void	*ptr;
size_t	 size, n;
FILE	*file;
{
	if (file == NULL || file->readfn == NULL) {
		errno = EINVAL;
		return EOF;
	}

	return file->readfn (file->cookie, ptr, size * n);
}
