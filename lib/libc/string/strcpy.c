#include <string.h>

char *
strcpy (d, s)
char		*d;
const char	*s;
{
	char *o = d;
	while (*s != '\0')
		*d++ = *s++;
	return o;
}
