#include <_varargs.h>
#include <stdio.h>

int
snprintf (s, num, fmt)
char		*s;
size_t		 num;
const char	*fmt;
{
	va_list	ap;
	int	n;

	va_start (ap, fmt);
	n = vsnprintf (s, num, fmt, ap);
	va_end (ap);

	return n;
}
