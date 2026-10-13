#include <stdlib.h>
#include <ctype.h>

static int
parse_digit (ch, d, base)
int	ch, *d, base;
{
	if (isdigit (ch)) {
		*d = ch - '0';
	} else if (isupper (ch)) {
		*d = ch - 'A' + 10;
	} else if (islower (ch)) {
		*d = ch - 'a' + 10;
	} else {
		return 0;
	}

	return *d < base;
}

int
strtoi (s, endp, base)
const char	 *s;
char		**endp;
int		  base;
{
	int	neg = 0, v, d;

	if (*s == '-') {
		neg = 1;
		++s;
	} else if (*s == '+') {
		++s;
	}

	for (v = 0; parse_digit (*s, &d, base); ++s)
		v = (v * base) + d;

	if (endp != NULL)
		*endp = (char *)s;

	return neg ? -v : v;
}
