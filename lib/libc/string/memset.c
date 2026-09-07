#include <string.h>

void *
memset (void *dest, int val, size_t num)
{
	size_t i;
	for (i = 0; i < num; ++i)
		((unsigned char *)dest)[i] = val;
	return dest;
}
