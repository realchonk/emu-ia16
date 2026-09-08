#include <stdio.h>
#include <errno.h>

int
fflush (file)
FILE *file;
{
	if (file == NULL) {
		/* TODO: flush all open output streams */
	}
	return 0;
}
