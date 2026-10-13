#include <string.h>

int
strncmp (s1, s2, len)
const char	*s1, *s2;
size_t		 len;
{
	size_t i;

	for (i = 0; i < len && s1[i] == s2[i] && s1[i] != '\0'; ++i);
	return i < len ? s1[i] - s2[i] : 0;
}
