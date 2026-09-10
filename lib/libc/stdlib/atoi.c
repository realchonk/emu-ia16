#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

int
atoi (s)
const char *s;
{
	bool	neg = false;
	int	x;

	if (*s == '-') {
		neg = true;
		++s;
	}

	for (x = 0; isdigit (*s); ++s)
		x = x * 10 + (*s - '0');

	return neg ? -x : x;
}
