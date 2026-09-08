#include <stdlib.h>
#include <stdio.h>

int
fclose (file)
FILE *file;
{
	int ec;
	fflush (file);
	ec = file->closefn != NULL ? file->closefn (file->cookie) : 0;
	free (file);
	return ec;
}
