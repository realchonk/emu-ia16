#include <string.h>

void *
memcpy (void *restrict dest, const void *restrict src, size_t num)
{
	size_t i;
	for (i = 0; i < num; ++i)
		((unsigned char *)dest)[i] = ((const unsigned char *)src)[i];
	return dest;
}
