#include <string.h>

char *
strcpy (d, s)
char		*d;
const char	*s;
{
	char *o = d;
	do {
		*d++ = *s;
	} while (*s++ != '\0');
	return o;
}
