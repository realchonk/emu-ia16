#include <string.h>

char *
strrchr (s, c)
const char	*s;
int		 c;
{
	char *l = NULL;
	for (; *s != '\0'; ++s) {
		if (*s == c)
			l = s;
	}
	return (char *)l;
}
