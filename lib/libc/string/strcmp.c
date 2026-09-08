#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int
strcmp (s1, s2)
const char *s1, *s2;
{
	size_t l1, l2;
	l1 = strlen (s1);
	l2 = strlen (s2);
	return memcmp (s1, s2, MIN (l1, l2) + 1);
}
