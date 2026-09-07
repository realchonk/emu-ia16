#include <stdlib.h>
#include <string.h>

void *
calloc (n, s)
size_t n, s;
{
	void *p;

	p = malloc (n * s);
	if (p != NULL)
		memset (p, 0, n * s);
	return p;
}
