#include <stdlib.h>
#include <string.h>

char *
strndup (s, l)
const char	*s;
size_t		 l;
{
	size_t	 n;
	char	*c;

	n = strnlen (s, l);
	c = malloc (n + 1);
	if (c != NULL)
		memcpy (c, s, n + 1);
	return c;
}

