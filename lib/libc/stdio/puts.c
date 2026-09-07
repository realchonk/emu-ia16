#include <unistd.h>
#include <string.h>
#include <stdio.h>

int
puts (s)
const char *s;
{
	char nl = '\n';

	if (write (1, s, strlen (s)) < 0)
		return EOF;
	return write (1, &nl, 1) == 1 ? 0 : -1;
}
