#include <stddef.h>

size_t
strlen (const char *s)
{
	size_t i;
	for (i = 0; *s != '\0'; ++s, ++i);
	return i;
}
