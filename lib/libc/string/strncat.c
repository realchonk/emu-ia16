#include <string.h>

char *
strncat (d, s, n)
char		*d;
const char	*s;
size_t		 n;
{
	size_t ld, ls;
	ld = strlen (d);
	ls = strnlen (s, n);
	memcpy (d + ld, s, ls);
	d[ld + ls] = '\0';
	return d;
}
