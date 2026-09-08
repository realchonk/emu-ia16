#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

FILE *
funopen (cookie, readfn, writefn, seekfn, closefn)
void	 *cookie;
int	(*readfn)(), (*writefn)(), (*closefn)();
long	(*seekfn)();
{
	FILE *file;

	if (readfn == NULL && writefn == NULL) {
		errno = EINVAL;
		return NULL;
	}

	file = malloc (sizeof (FILE));
	if (file == NULL)
		return NULL;

	file->cookie	= cookie;
	file->peekc	= EOF;
	file->readfn	= readfn;
	file->writefn	= writefn;
	file->seekfn	= seekfn;
	file->closefn	= closefn;

	return file;
}
