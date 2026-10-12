#include <string.h>

size_t
strcspn (s, cs)
const char	*s, *cs;
{
	size_t i;

	for (i = 0; *s != '\0' && strchr (cs, *s) == NULL; ++i, ++s);

	return i;
}
