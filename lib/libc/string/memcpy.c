#include <string.h>

void *
memcpy (dest, src, num)
void		*dest;
const void	*src;
size_t		 num;
{
	size_t i;
	for (i = 0; i < num; ++i)
		((unsigned char *)dest)[i] = ((const unsigned char *)src)[i];
	return dest;
}
