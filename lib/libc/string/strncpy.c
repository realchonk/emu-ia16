#include <string.h>

char *
strncpy (d, s, n)
char		*d;
const char	*s;
size_t		 n;
{
	char *o = d;
	for (; n != 0 && *s != '\0'; --n)
		*d++ = *s++;
	for (; n != 0; --n)
		*d++ = '\0';
	return o;
}
