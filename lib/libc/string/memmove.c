#include <string.h>

void *
memmove (dest, src, num)
void		*dest;
const void	*src;
size_t		 num;
{
	unsigned char		*d = dest;
	const unsigned char	*s = src;
	size_t			 i;

	if (dest < src) {
		for (i = 0; i < num; ++i)
			d[i] = s[i];
	} else if (dest > src) {
		for (i = num; i != 0; --i)
			d[i-1] = s[i-1];
	}

	return dest;
}
