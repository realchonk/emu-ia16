#include <string.h>

int
memcmp (p1, p2, n)
const void	*p1, *p2;
size_t		 n;
{
	const unsigned char *s1 = p1, *s2 = p2;
	for (; n != 0 && *s1 == *s2; ++s1, ++s2, --n);
	return n > 0 ? *s1 - *s2 : 0;
}
