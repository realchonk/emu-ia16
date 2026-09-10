#include <stdlib.h>
#include <string.h>

char *
strdup (s)
const char *s;
{
	size_t	 n;
	char	*c;

	n = strlen (s);
	c = malloc (n + 1);
	if (c != NULL)
		memcpy (c, s, n + 1);
	return c;
}
