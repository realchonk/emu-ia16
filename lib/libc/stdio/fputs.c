#include <string.h>
#include <stdio.h>

int
fputs (s, file)
const char	*s;
FILE		*file;
{
	return fwrite (s, 1, strlen (s), file) > 0 ? 0 : EOF;
}
