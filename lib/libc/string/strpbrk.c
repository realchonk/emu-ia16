#include <string.h>

char *
strpbrk (s, cs)
const char	*s, *cs;
{
	for (; *s != '\0'; ++s) {
		if (strchr (cs, *s) != NULL)
			return (char *)s;
	}
	return NULL;
}
